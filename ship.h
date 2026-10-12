

/* prototypes for all the routines which define 
 * an interface that allows control of the ship
 */

void ShipSteerLeft(int ship, int flag) ;
void ShipSteerRight(int ship, int flag) ;
void ShipFire(int ship, int flag) ;
void ShipThrust(int ship, int flag) ;

typedef struct t_shipstatus {
	int x[2], y[2], dx[2], dy[2] ;
	float th[2] ;
} ShipStatus ;

void ShipStrategy(int me, int him, ShipStatus *status) ;
