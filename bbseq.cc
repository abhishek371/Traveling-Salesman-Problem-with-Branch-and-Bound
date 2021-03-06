/* ******************************************************************** */
/*               Algoritmo Branch-And-Bound Secuencial                  */
/* ******************************************************************** */
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include "mpi.h"
#include "libbb.h"

using namespace std;

unsigned int NCIUDADES;
int rank, size;
char salida_ini[14];
char salida_fin[7];

main (int argc, char **argv) {

	////
	// Iniciar MPI.
	////
	MPI_Init(&argc, &argv);
	inicializar_estado_proceso(true);

	////
	// Control de errores de los datos de entrada.
	////
	switch (argc) {
		case 3:		NCIUDADES = atoi(argv[1]);
					break;

		default:	cerr << "La sintaxis es: bbseq <tam.> <archivo>" << endl;
					MPI_Finalize();
					exit(1);
					break;
	}

	////
	// Matriz que contiene los datos que se lean del fichero.
	////
	int** tsp0;

	////
	// Nodos empleados en el algoritmo.
	////
	////// Nodo a explorar.
	tNodo	nodo,
	////// Hijo izquierdo.
			lnodo,
	////// Hijo derecho.
			rnodo,
	////// Mejor solucion.
			solucion;
	////
	// Condición de fin.
	////
	bool activo,
	////
	// Hay disponible un nuevo valor de cota superior.
	////
		nueva_U;
	////
	//	Valor de la cota superior.
	////
	int  U;
	////
	// Número de iteraciones.
	////
	int iteraciones = 0;
	////
	// Pila de nodos pendientes de explorar.
	////
	tPila pila;

	////
	// Variable usada para la medida de tiempos.
	////
	double t=-1.0;


	////
	// Inicialización del algoritmo
	////
	////// Inicializa la cota superior.
	U = INFINITO;
	////// Inicializa la estructura de datos 'nodo'.
	InicNodo (&nodo);

	if(rank == 0) {
		// Se reserva memoria
		tsp0 = reservarMatrizCuadrada(NCIUDADES);
		////// Lee los datos a partir del archivo en la matriz 'tsp0'.
		LeerMatriz (argv[2], tsp0, true);
		////// Indica si la matriz leida del fichero es correcta y en función de ello
		////// marca como activa la fase Branch&Bound. Si la matriz es inconsistente
		////// el algoritmo no iniciará.
		activo = !Inconsistente(tsp0);
	}

	////// Se informa al resto de procesos si se continua o no.
	MPI_Bcast(&activo, 1, MPI_BYTE, 0, MPI_COMM_WORLD);

	////// Si hay que continuar:
	if(activo) {
		////// El resto de procesos reservan memoria.
		if(rank != 0)	tsp0 = reservarMatrizCuadrada(NCIUDADES);

		////// Se distribuye la matriz con los datos.
		MPI_Bcast(&tsp0[0][0], NCIUDADES*NCIUDADES, MPI_INT, 0, MPI_COMM_WORLD);

		////// Los procesos que no son el P0 llaman a la función de equilibrado
		////// de carga ya que actualmente no tienen trabajo.
		if(rank != 0) Equilibrar_Carga(&pila, &activo, true);


		////// Inicia la medida de tiempo.
	    t=MPI::Wtime();
		////
		// Ciclo Branch&Bound.
		////
		////// Mientras esté activo el ciclo.
		while (activo) {
			////// Obtiene el hijo izquierdo y derecho del 'nodo' según tsp0.
			Ramifica (&nodo, &lnodo, &rnodo, tsp0);
			////// Indica que no hay nueva cota superior.	
			nueva_U = false;
			////// Si el hijo derecho es una solución válida:
			if (Solucion(&rnodo)) {
				////// y además su cota inferior es menor que la cota superior actual:
				////// (Se ha encontrado una solución mejor)
				if (rnodo.ci() < U) {
					////// Se actualiza la cota superior.
					U = rnodo.ci();
					////// Se indica que hay una nueva cota superior.
					nueva_U = true;
					////// Guardamos el hijo derecho como nuevo nodo solución.
					CopiaNodo (&rnodo, &solucion);
				}
			}
			////// Si el hijo derecho no es una solución válida,
			else {
				////// y su cota inferior es mejor que la cota superior actual,
				if (rnodo.ci() < U) {
					////// Introduce el hijo derecho en la pila.
					if (!pila.push(rnodo)) {
						////// Si la pila está llena (pila.push() = false) y no se puede insertar el nodo
						////// el programa termina.
						printf ("Error: pila agotada\n");
						liberarMatriz(tsp0);
						exit (1);
					}
				}
			}
			////// Se realiza el mismo proceso para el hijo izquierda en este caso.
			////// En primer lugar se comprueba si es solución. De ser así,
			if (Solucion(&lnodo)) {
				////// y además su cota inferior es menor que la cota superior actual:
				////// (Se ha encontrado una solución mejor)
				if (lnodo.ci() < U) {
					////// Se actualiza la cota superior.
					U = lnodo.ci();
					////// Se indica que hay una nueva cota superior.
					nueva_U = true;
					////// Guardamos el hijo derecho como nuevo nodo solución.
					CopiaNodo (&lnodo,&solucion);
				}
			}
			////// Si el hijo izquierdo no es una solución válida,
			else {
				////// y su cota inferior es mejor que la cota superior actual,
				if (lnodo.ci() < U) {
					////// Introduce el hijo izquierdo en la pila.
					if (!pila.push(lnodo)) {
						////// Si la pila está llena (pila.push() = false) y no se puede insertar el nodo
						////// el programa termina.
						printf ("Error: pila agotada\n");
						liberarMatriz(tsp0);
						exit (1);
					}
				}
			}
			
			////// Difusion de la cota superior
			nueva_U = Difusion_Cota_Superior(&U, nueva_U);

			////// Si en la iteración actual se ha encontrado una nueva cota superior.
			////// se acota la pila de nodos usando la nueva cota superior.
			if (nueva_U) pila.acotar(U);

			////// Llamamos al método de equilibrado de carga.
			Equilibrar_Carga(&pila, &activo, true);

			////// El ciclo Branch&Bound permanecerá activo mientras se pueda extraer un
			////// nuevo nodo de la pila.
			if(activo)	activo = pila.pop(nodo);
			////// Llevamos la cuenta del número de iteraciones empleado usando un contador.
			iteraciones++;
		}
		////// Contabilizamos el tiempo empleado.
	    t=MPI::Wtime()-t;

		////// Se imprime la solución.
		//if(rank==0)
		//{
			printf ("Solucion: \n");
			EscribeNodo(&solucion);
		    cout<<salida_ini<< "Tiempo gastado= "<<t<<endl;
			cout << "Numero de iteraciones = " << iteraciones <<salida_fin<< endl << endl;
		//}
		liberarMatriz(tsp0);
	}
	else if(rank==0){
		cout<<"Matriz de entrada inconsistente."<<endl;
		liberarMatriz(tsp0);
	}

	////// Liberamos los recursos empleados por la interfaz de paso de mensajes MPI.
    MPI::Finalize();
}
