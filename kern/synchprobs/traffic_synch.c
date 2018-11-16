#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>

#include <array.h>

/* 
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */

/* 
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 */



struct car {
  Direction from;
  Direction to;
};
typedef struct car car;
struct lock *car_lk;
struct cv * car_cv;
struct array* cars;
bool not_collide(car* inside, car* outside);
bool can_enter(car* outside);

bool not_collide(car* inside, car* outside) {
  if (inside->from == outside->from || (inside->from == outside->to && inside->to == outside->from)) {
    return true;
  }
  else if (inside->to != outside->to && (((inside->from == north && inside->to == west) ||
                                         (inside->from == east && inside->to == north) ||
                                         (inside->from == south && inside->to == east) ||
                                         (inside->from == west && inside->to == south))
                                         ||
                                         ((outside->from == north && outside->to == west) ||
                                         (outside->from == east && outside->to == north) ||
                                         (outside->from == south && outside->to == east) ||
                                         (outside->from == west && outside->to == south)))) {
                                           return true;
                                         }
  else {
    return false;
  }
}

bool can_enter(car* outside) {
  for (unsigned int i = 0; i < array_num(cars); i++) {
    if(not_collide(array_get(cars, i), outside) == false) {
      return false;
    }
  }
  return true;
}


/* 
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables.
 * 
 */
void
intersection_sync_init(void)
{
  /* replace this default implementation with your own implementation */

  cars = array_create();
  array_init(cars);

  car_lk = lock_create("cars_lk");
  car_cv = cv_create("cars_cv");

  return;
}

/* 
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 *
 */
void
intersection_sync_cleanup(void)
{
  /* replace this default implementation with your own implementation */
  KASSERT(cars != NULL);
  KASSERT(car_lk != NULL);
  KASSERT(car_cv != NULL);
  array_destroy(cars);
  lock_destroy(car_lk);
  cv_destroy(car_cv);
}


/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread 
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */

void
intersection_before_entry(Direction origin, Direction destination) 
{
  /* replace this default implementation with your own implementation */
  KASSERT(cars != NULL);
  KASSERT(car_lk != NULL);
  KASSERT(car_cv != NULL);

  lock_acquire(car_lk);

  car* cur_car = kmalloc(sizeof(struct car));
  cur_car->from = origin;
  cur_car->to = destination;

  while(!can_enter(cur_car)) {
    cv_wait(car_cv, car_lk);
  }
  
  array_add(cars, cur_car, NULL);
  lock_release(car_lk);
}


/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
 * return value: none
 */

void
intersection_after_exit(Direction origin, Direction destination) 
{
  /* replace this default implementation with your own implementation */
  KASSERT(cars != NULL);
  KASSERT(car_lk != NULL);
  KASSERT(car_cv != NULL);

  lock_acquire(car_lk);

  for (unsigned int i = 0; i < array_num(cars); i++) {
    car* cur_car = array_get(cars, i);
    if (cur_car->to == destination && cur_car->from == origin) {
      array_remove(cars, i);
      cv_broadcast(car_cv, car_lk);
      break;
    }
  }

  lock_release(car_lk);
}
