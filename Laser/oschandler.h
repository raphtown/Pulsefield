#pragma once

#include <vector>
#include <string>
#include <pthread.h>
#include "lo/lo.h"
#include "dest.h"
#include "drawing.h"
#include "laser.h"

class Video;

class OSCHandler {
    Drawing drawing;
    Lasers lasers;
    Video *video;
    int serverPort;

    int unit;
    Destinations dests;
    lo_server s;
    
    pthread_t incomingThread;
    static void *processIncoming(void *arg);

    Color currentColor;
    float currentDensity;
    int npoints;
 public:
    OSCHandler(const Lasers &lasers, Video *video);
    ~OSCHandler();

    void run();
    void wait() {
	// Wait till done
	pthread_join(incomingThread,NULL);
    }
    // Handlers for OSC messages
    void startStop(bool start);
    // Destination handling
    void addDest(const char *host, int port);
    void addDest(lo_message msg, int port);
    void rmDest(const char *host, int port);
    void rmDest(lo_message msg, int port);
    void rmAllDest();
    void ping(lo_message msg, int seqnum);

    // Laser settings
    void setPPS(int unit,int pps);
    void setPoints(int unit,int points);	// Target points per frame
    void setBlanking(int unit,int before, int after);
    void setSkew(int unit, int s);

    // Attributes
    void setColor(Color c);
    void setDensity(float d);
    void setAttribute(const char *attr, float value);

    // Primitives
    void circle(Point center, float radius);
    void arc(Point center, Point perim,  float angleCW);
    void line(Point p1, Point p2);
    void cubic(Point p1, Point p2, Point p3, Point p4);

    void update();
    void map(int unit, Point world, Point local);
    void setTransform(int unit);

    // /pf/frame
    float minx,maxx,miny,maxy;
    void pfframe();
};