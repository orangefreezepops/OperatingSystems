#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

#include "museumsim.h"

//
// In all of the definitions below, some code has been provided as an example
// to get you started, but you do not have to use it. You may change anything
// in this file except the function signatures.
//

struct shared_data {
	// Add any relevant synchronization constructs and shared state here.
	// For example:
	pthread_mutex_t ticket_mutex;
	pthread_cond_t cv_guide;
	pthread_cond_t cv_visitor;
	pthread_cond_t cv_full_museum;
	pthread_cond_t cv_empty_museum;
	int tickets;
	int visitorsGivenTours;
	int visitorsInMuseum;
	int visitorsOutside;
	int totalVisitors;
	int guidesInMuseum;
	int guidesOutside;
	int totalGuides;
};

static struct shared_data shared;


/**
 * Set up the shared variables for your implementation.
 * 
 * `museum_init` will be called before any threads of the simulation
 * are spawned.
 */
void museum_init(int num_guides, int num_visitors)
{
	pthread_mutex_init(&shared.ticket_mutex, NULL);
	pthread_cond_init(&shared.cv_guide, NULL);
	pthread_cond_init(&shared.cv_visitor, NULL);
	pthread_cond_init(&shared.cv_full_museum, NULL);
	pthread_cond_init(&shared.cv_empty_museum, NULL);
	shared.visitorsGivenTours = 0;
	shared.visitorsInMuseum = 0;
	shared.visitorsOutside = num_visitors;
	shared.totalVisitors = num_visitors;
	shared.guidesInMuseum = 0;
	shared.guidesOutside = num_guides;
	shared.totalGuides = num_guides;
	shared.tickets = MIN(VISITORS_PER_GUIDE * num_guides, num_visitors);
}


/**
 * Tear down the shared variables for your implementation.
 * 
 * `museum_destroy` will be called after all threads of the simulation
 * are done executing.
 */
void museum_destroy()
{
	pthread_mutex_destroy(&shared.ticket_mutex);
	pthread_cond_destroy(&shared.cv_guide);
	pthread_cond_destroy(&shared.cv_visitor);
	pthread_cond_destroy(&shared.cv_full_museum);
	pthread_cond_destroy(&shared.cv_empty_museum);
}

/**
 * Implements the visitor arrival, touring, and leaving sequence.
 */
void visitor(int id)
{
	visitor_arrives(id); //right away
	
	pthread_mutex_lock(&shared.ticket_mutex);			//lock the ticket lock to protect ticket number
	if (shared.tickets == 0){ 							//if there are no more tickets
		visitor_leaves(id); 							//leave immediately
		return;
	}

	//while the museum is at capacity OR there are no guides
	while((shared.visitorsGivenTours == shared.guidesInMuseum * VISITORS_PER_GUIDE) || (shared.guidesInMuseum == 0)){
		//wait for them to show up and let visitor in
		pthread_cond_wait(&shared.cv_guide, &shared.ticket_mutex);
	}
	//pthread_cond_broadcast(&shared.cv_visitor); //announce arrival
	printf("DEBUG: About to tour");	
	//visit has been admitted
	shared.visitorsGivenTours ++;	//increment number of tours given
	shared.visitorsInMuseum ++;		//incriment number of visitors in museum

	pthread_mutex_unlock(&shared.ticket_mutex); //unlock so the visitors can tour synchronously

//-----------------------------UNPROTECTED---------------------------------------------------	
	visitor_tours(id); //tour independently
//-----------------------------UNPROTECTED---------------------------------------------------

	pthread_mutex_lock(&shared.ticket_mutex); 	//relock to protect shared data

	visitor_leaves(id); 			//leave wheneva 
	shared.visitorsInMuseum --;		//decrement number of visitors in museum 
	
	if (shared.visitorsInMuseum == 0){
		//museum is empty again
		//guides can leave
		pthread_cond_broadcast(&shared.cv_empty_museum);
	}
	
	pthread_mutex_unlock(&shared.ticket_mutex);//unlock ticket mutex
	
}

/**
 * Implements the guide arrival, entering, admitting, and leaving sequence.
 */
void guide(int id)
{
	guide_arrives(id); //right away
	pthread_mutex_lock(&shared.ticket_mutex); //protect the guide

	while (shared.guidesInMuseum == GUIDES_ALLOWED_INSIDE){
		pthread_cond_wait(&shared.cv_full_museum, &shared.ticket_mutex);
	}

	guide_enters(id); 							//guide enters
	pthread_cond_broadcast(&shared.cv_guide); 	//broadcast they arrived
	shared.guidesInMuseum ++;					//number in museum increases

	//while the guide hasn't given all the tours they possibly can
	while (shared.visitorsGivenTours < MIN((shared.guidesInMuseum * VISITORS_PER_GUIDE), shared.totalVisitors)){
		//if there aren't any visitors
		while(shared.visitorsOutside == 0 && shared.visitorsInMuseum == 0){
			//wait til they show up
			pthread_cond_wait(&shared.cv_visitor, &shared.ticket_mutex);
		}
		if (shared.tickets > 0){
			shared.tickets --;						//hand out ticket
			guide_admits(id); 						//admit the visitors
			
		}
		pthread_cond_broadcast(&shared.cv_guide); 	//signal they can come in
	}

	while(shared.tickets == 0){
		pthread_cond_wait(&shared.cv_empty_museum, &shared.ticket_mutex);
	}
	guide_leaves(id);
	pthread_cond_signal(&shared.cv_full_museum);
	pthread_mutex_unlock(&shared.ticket_mutex);
}

