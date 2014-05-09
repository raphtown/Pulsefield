#include <iostream>
#include <cmath>
#include <math.h>

#include "etherdream.h"
#include "laser.h"
#include "dbg.h"
#include "point.h"

static const int MAXSLEWDISTANCE=65535/20;

Laser::Laser(int _unit) {
    unit=_unit;
    PPS=30000;
    npoints=600;
}

int Laser::open() {
    etherdream_lib_start();

    /* Sleep for a bit over a second, to ensure that we see broadcasts
     * from all available DACs. */
    usleep(1200000);

    int cc = etherdream_dac_count();
    if (!cc) {
	printf("No DACs found.\n");
	return -1;
    }

    int i;
    for (i = 0; i < cc; i++) {
	printf("%d: Ether Dream %06lx\n", i,etherdream_get_id(etherdream_get(i)));
    }

    if (cc<=unit) {
	printf("Requested laser unit %d, but only have %d lasers\n", unit, cc);
	return -1;
    }
    d = etherdream_get(unit);
    printf("Connecting to laser %d...\n",unit);
    if (etherdream_connect(d) < 0)
	return -1;
    return 0;
}

void Laser::update() {
    if (pts.size() < 2)  {
	std::cerr << "Laser::update: not enough points (" << pts.size() << ") -- not updating" << std::endl;
	return;
    }

    if (d==0) {
	std::cerr << "Laser not open" << std::endl;
	return;
    }
    dbg("Laser.update",1) << "Wait for ready." << std::endl;
    etherdream_wait_for_ready(d);
    dbg("Laser.update",1) << "Sending " << pts.size() << " points" << std::endl;
    int res = etherdream_write(d,pts.data(), pts.size(), PPS, -1);
    if (res != 0)
	printf("write %d\n", res);
}

// Get number of needed blanks for given slew
std::vector<etherdream_point> Laser::getBlanks(etherdream_point initial, etherdream_point final) {
    std::vector<etherdream_point>  result;
    // Calculate distance in device coords
    int devdist=std::max(abs(initial.x-final.x),abs(initial.y-final.y));
    if (devdist>0) {
	int nblanks=std::ceil(devdist/MAXSLEWDISTANCE)+10;
	dbg("Drawing.getPoints",2) << "Inserting " << nblanks << " for a slew of distance " << devdist << " from " << initial.x << "," << initial.y << " to " << final.x << "," << final.y << std::endl;
	etherdream_point blank=final;
	blank.r=0; blank.g=0;blank.b=0;
	for (int i=0;i<nblanks;i++) 
	    result.push_back(blank);
    }
    return result;
}


void Laser::render(const Drawing &drawing) {
    pts=drawing.getPoints(npoints,transform);
    dbg("Laser.render",2) << "Rendered drawing into " << pts.size() << " points." << std::endl;
    if (pts.size()>=2)
	update();
}

Lasers::Lasers(int nlasers): lasers(nlasers) {
    for (unsigned int i=0;i<lasers.size();i++) {
	lasers[i]=new Laser(i);
	lasers[i]->open();
    }
}

Lasers::~Lasers() {
    for (unsigned int i=0;i<lasers.size();i++)
	delete lasers[i];
}

void Lasers::refresh() {
    for (unsigned int i=0;i<lasers.size();i++)
	lasers[i]->render(drawing);
}
