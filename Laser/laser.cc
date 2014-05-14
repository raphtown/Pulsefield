#include <iostream>
#include <cmath>
#include <math.h>

#include "etherdream_bst.h"
#include "laser.h"
#include "dbg.h"
#include "point.h"

static const int MAXSLEWDISTANCE=65535/20;

Laser::Laser(int _unit): labelColor(0,0,0),maxColor(0,1,0) {
    unit=_unit;
    PPS=30000;
    npoints=600;
    labelColor=Color::getBasicColor(unit);
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
	dbg("Laser.update",1) << "Laser::update: not enough points (" << pts.size() << ") -- not updating" << std::endl;
	return;
    }

    if (d==0) {
	dbg("Laser.update",1) << "Laser not open" << std::endl;
	return;
    }
    dbg("Laser.update",1) << "Wait for ready." << std::endl;
    etherdream_wait_for_ready(d);
    dbg("Laser.update",1) << "Sending " << pts.size() << " points at " << PPS << " pps"  << std::endl;
    int res = etherdream_write(d,pts.data(), pts.size(), PPS, -1);
    dbg("Laser.update",1) << "Finished writing to etherdream, res=" << res << std::endl;
    if (res != 0)
	printf("write %d\n", res);
}

// Get number of needed blanks for given slew
std::vector<etherdream_point> Laser::getBlanks(etherdream_point initial, etherdream_point final) {
    std::vector<etherdream_point>  result;
    if (initial.r==0 &&initial.g==0 && initial.b==0 && final.r==0 &&final.g==0 && final.b==0) {
	dbg("Drawing.getBlanks",2) << "No blanks needed; initial or final are already blanked" << std::endl;
	return result;
    }
    // Calculate distance in device coords
    int devdist=std::max(abs(initial.x-final.x),abs(initial.y-final.y));
    if (devdist>0) {
	int nblanks=std::ceil(devdist/MAXSLEWDISTANCE)+10;
	dbg("Drawing.getBlanks",2) << "Inserting " << nblanks << " for a slew of distance " << devdist << " from " << initial.x << "," << initial.y << " to " << final.x << "," << final.y << std::endl;
	etherdream_point blank=final;
	blank.r=0; blank.g=0;blank.b=0;
	for (int i=0;i<nblanks;i++) 
	    result.push_back(blank);
    }
    return result;
}


void Laser::render(const Drawing &drawing) {
    if (d!=0) {
	int fullness=etherdream_getfullness(d);
	dbg("Laser.render",2) << "Fullness=" << fullness << std::endl;
	if (fullness>1) {
	    dbg("Laser.render",2) << "Etherdream already busy enough with " << fullness << " frames -- not rendering new drawing " << std::endl;
	    return;   
	}
    }
    pts=drawing.getPoints(npoints,transform,spacing);
    dbg("Laser.render",2) << "Rendered drawing into " << pts.size() << " points with a spacing of " << spacing << std::endl;
    if (pts.size()>=2)
	update();
}

Lasers::Lasers(int nlasers): lasers(nlasers) {
    for (unsigned int i=0;i<lasers.size();i++) {
	lasers[i]=std::shared_ptr<Laser>(new Laser(i));
	lasers[i]->open();
    }
    needsRender=true;
    if (pthread_mutex_init(&mutex,NULL)) {
	std::cerr<<"Failed to create lasers mutex" << std:: endl;
	exit(1);
    }
}

Lasers::~Lasers() {
    (void)pthread_mutex_destroy(&mutex);
}

int Lasers::render() {
    if (!needsRender) {
	dbg("Lasers.render",5) << "Not dirty" << std::endl;
	return 0;
    }
    Drawing dtmp=drawing; // Copy in case there are updates
    needsRender=false;   // TODO: Should be atomic with prior statement
    lock();
    for (unsigned int i=0;i<lasers.size();i++)
	lasers[i]->render(dtmp);
    dbg("Lasers.render",1) << "Render done" << std::endl;
    return 1;
}

void Lasers::setDrawing(const Drawing &_drawing) {
    if (_drawing.getFrame() == drawing.getFrame()) {
	dbg("Lasers.setDrawing",1) << "Multiple drawings for same frame: " << drawing.getFrame() << std::endl;
	return;
    }
    lock();
    drawing=_drawing;
    dbg("Lasers.setDrawing",2) << "Frame=" << drawing.getFrame() << std::endl;
    needsRender=true;
    unlock();
}

void Lasers::saveTransforms(std::ostream &s)  const {
    for (unsigned int i=0;i<lasers.size();i++)
	lasers[i]->getTransform().save(s);
}

void Lasers::loadTransforms(std::istream &s) {
    for (unsigned int i=0;i<lasers.size();i++)
	lasers[i]->getTransform().load(s);
    needsRender=true;
}

void Lasers::lock() {
    dbg("Lasers.lock",5) << "lock req" << std::endl;
    if (pthread_mutex_lock(&mutex)) {
	std::cerr << "Failed call to pthread_mutex_lock" << std::endl;
	exit(1);
    }
    dbg("Lasers.lock",5) << "lock acquired" << std::endl;
}

void Lasers::unlock() {
    dbg("Lasers.unlock",5) << "unlock" << std::endl;
    if (pthread_mutex_unlock(&mutex)) {
	std::cerr << "Failed call to pthread_mutex_lock" << std::endl;
	exit(1);
    }
}

