#include <iomanip>
#include <cmath>
#include <math.h>
#include <assert.h>
#include <algorithm>

#include "lo/lo.h"
#include "legstats.h"
#include "leg.h"
#include "parameters.h"
#include "dbg.h"
#include "normal.h"
#include "vis.h"
#include "lookuptable.h"

static const bool USEMLE=false;  // True to use MLE from likelihood grid; otherwise use mean
// New predicted position is weighted sum of prior positions from current and other leg
// sum(same)+sum(other) should equal 1.0 (or would result in a net drift towards/away from origin
    // From results of optimization using optpredict.m
std::vector<float> Leg::samePredictWeights;
std::vector<float> Leg::otherPredictWeights;

void Leg::setup(float fps, int nlidar) {
    static const float stridePeriod=1.22;   // Stride period in seconds
    const int strideFrames=(int)(stridePeriod*fps*nlidar+0.5);   // Number of frames for a complete stride
    dbg("Leg.setup",1) << "Setting strideFrame to " << strideFrames << std::endl;

    // Simple zero-order hold
    //samePredictWeights.resize(1);
    //samePredictWeights[0]=1.0;
    float same[]={0.7561,0.3262,0.0190,-0.0110,-0.0025,0.0006,0.0066,-0.0408,0.0072,-0.0718};   // weight prior positions to predict from current leg
    float other[]={0.0376,0.0006,0.0045,-0.0001,0.0038,-0.0052,0.0038,0.0005,-0.0229,-0.0124};   // from other leg
    samePredictWeights.assign(same,same+sizeof(same)/sizeof(same[0]));
    otherPredictWeights.assign(other,other+sizeof(other)/sizeof(other[0]));
    return;
#if 0    
    const float crossLIDARWeight=0.0f;   // How much to weigh data from other LIDARs
    const int nweights=50;
    predictWeights.resize(nweights);

    // Initial weight is best predictor of a sine wave offset by 1/strideFrames of a cycle

    predictWeights[0]=cos(2*M_PI/strideFrames);
    predictWeights[1]=sin(2*M_PI/strideFrames);
    const float totalDamping=0.9864;
    // damp things so legs reach equal predicted velocity in 1/4 stride
    const float sameDamping=pow(2.0,-1.0f/(strideFrames/4))*totalDamping;
    dbg("Leg.setup",1) << "Damping = " << totalDamping << ", " << sameDamping << std::endl;
    for (int i=2;i<nweights;i+=2) {
	predictWeights[i]=predictWeights[i-2]*sameDamping;
	predictWeights[i+1]=(predictWeights[i-2]+predictWeights[i-1])*totalDamping-predictWeights[i];
	if (predictWeights[i+1]>predictWeights[i]) {
	    predictWeights[i]=(predictWeights[i]+predictWeights[i+1])/2;
	    predictWeights[i+1]=predictWeights[i];
	}
    }
#endif

    // Make sum of all weights add up to 1.0
    float sumS=0,sumO=0;
    for (int i=0;i<samePredictWeights.size();i++)
	sumS+=samePredictWeights[i];
    for (int i=0;i<otherPredictWeights.size();i++)
	sumO+=otherPredictWeights[i];
    dbg("Leg.setup",1) << "sum=" << sumS << "+" << sumO << "=" << sumS+sumO << std::endl;

    // Parameters can be tested using optimalveldamping2.m 
    dbg("Leg.setup",1) << "samePredictWeights=[";
    for (int i=0;i<samePredictWeights.size();i++) {
	samePredictWeights[i]/=(sumO+sumS);
	dbgn("Leg.setup",1) << samePredictWeights[i];
    }
    dbgn("Leg.setup",1) << "]" << std::endl;
    dbg("Leg.setup",1) << "otherPredictWeights=[";
    for (int i=0;i<otherPredictWeights.size();i++) {
	samePredictWeights[i]/=(sumO+sumS);
	dbgn("Leg.setup",1) << otherPredictWeights[i];
    }
    dbgn("Leg.setup",1) << "]" << std::endl;
}

Leg::Leg(const Point &pt) {
    position=pt;
    predictedPosition=pt;
    posvar=INITIALPOSITIONVAR;
    prevposvar=posvar;
    diam=INITLEGDIAM;
    diamSigma=LEGDIAMSIGMA;
    updateDiam=false;
    if (!updateDiam) {
	dbg("Leg",1) << "Not updating leg diameters" << std::endl;
    }
    consecutiveInvisibleCount=0;
    velocity=Point(0,0);

    like.clear();
    scanpts.clear();
    maxlike=nan("");
    likenx=0;
    likeny=0;
    minval=Point(nan(""),nan(""));
    maxval=Point(nan(""),nan(""));

}

// Empty constructor used to initialize array, gets overwritten using above ctor subsequently
Leg::Leg() {
    ;
}

std::ostream &operator<<(std::ostream &s, const Leg &l) {
    s << std::fixed << std::setprecision(0)
      << "pos: " << l.position << "+/-" << sqrt(l.posvar)
      << ", vel: " << l.velocity
      << ", diam:  " << l.diam
      << ", maxlike=" << l.maxlike
      << std::setprecision(3);
    return s;
}

// Predict next leg position from current one
// Use last known position from same LIDAR + weighted sum of prior changes in position (so opposite leg absolute position is ignored)
// Need to handle number of LIDAR used (prior positions are from different LIDAR)
void Leg::predict(const Leg &otherLeg) {
    Point newPosition(0,0);
    float rmse;
    for (int i=0;i<samePredictWeights.size();i++)
	newPosition=newPosition+samePredictWeights[i]*getPriorPosition(i+1);
    for (int i=0;i<otherPredictWeights.size();i++)
	newPosition=newPosition+otherPredictWeights[i]*otherLeg.getPriorPosition(i+1);
    //    rmse=newDelta.norm()*0.16+10;
    Point newDelta=newPosition-getPriorPosition(1);
    rmse=newDelta.norm()*0.08+7;

    dbg("Leg.predict",5) << "newDelta=" << newDelta << ", rmse=" << rmse << std::endl;

    position=newPosition;
    prevposvar=posvar;
    posvar=std::min(posvar+rmse*rmse,MAXPOSITIONVAR);
    predictedPosition=position;   // Save this before applying measurements for subsequent analyses

    // Clear out variables that are no longer valid
    like.clear();
    scanpts.clear();
    maxlike=nan("");
    likenx=0;
    likeny=0;
    minval=Point(nan(""),nan(""));
    maxval=Point(nan(""),nan(""));
}

void Leg::savePriorPositions() {
    priorPositions.push_back(position);
    // Keep the size under control
    if (priorPositions.size()>1000)
	priorPositions.erase(priorPositions.begin(),priorPositions.begin()+500);
}

Point Leg::getPriorDelta(int n) const {
    assert(n>0);
    if (n+1  > priorPositions.size())
	return Point(0,0);
    return priorPositions[priorPositions.size()-n]-priorPositions[priorPositions.size()-n-1];
}

Point Leg::getPriorPosition(int n) const {
    assert(n>0);
    if (n  > priorPositions.size()) {
	if (priorPositions.size() > 0)
	    return priorPositions[0];   // Return first point recorded
	else
	    return Point(0,0);
    }
    return priorPositions[priorPositions.size()-n];
}

// Get likelihood of an observed echo at pt hitting leg given current model
float Leg::getObsLike(const Point &pt, const Vis &vis,const LegStats &ls) const {
    float dpt=(pt-position).norm();
    float sigma=sqrt(pow(getDiamSigma()/2,2.0)+posvar);  // hack: use positition sigma inflated by leg diam variance
    // float like=normlike(dpt, getDiam()/2,sigma);
    Point delta=pt-position;
    Point diamOffset=delta/delta.norm()*getDiam()/2;
    float like=normlike(delta.X(),diamOffset.X(),sigma)+normlike(delta.Y(),diamOffset.Y(),sigma);
    // And check the likelihood that the echo is in front of the true leg position
    float frontlike=log(1-normcdf((pt-vis.getSick()->getOrigin()).norm(),(position-vis.getSick()->getOrigin()).norm(),sqrt(posvar)));  
    dbg("Leg.getObsLike",20) << "pt=" << pt << ", leg=" << position << ", pt-leg=" << (pt-position) << ", diam=" << getDiam() << ", dpt=" << dpt << ", sigma=" << sigma << ", like=" << like << ", frontlike=" << frontlike << std::endl;
    like+=frontlike;
    like=std::max((float)log(RANDOMPTPROB),like);
    assert(std::isfinite(like));
    return like;
}


void Leg::update(const Vis &vis, const std::vector<float> &bglike, const std::vector<int> fs,const LegStats &ls, const Leg *otherLeg) {
    // Copy in scanpts
    scanpts=fs;

    // Assume legdiam is log-normal (i.e. log(legdiam) ~ N(LOGDIAMMU,LOGDIAMSIGMA)
    const float LOGDIAMMU=log(getDiam());
    const float LOGDIAMSIGMA=log(1+getDiamSigma()/getDiam());

    dbg("Leg.update",5) << "prior: " << *this << std::endl;
    dbg("Leg.update",5) << " fs=" << fs << std::endl;
    

    // Find the rays that will hit this box
    // Use LIDAR local coordinates since we'll be mapping this to particular scans
    dbg("Leg.update",3) << "Box: " << vis.getSick()->worldToLocal(minval) << " - " << vis.getSick()->worldToLocal(maxval) << std::endl;
    float theta[4];
    theta[0]=vis.getSick()->worldToLocal(Point(maxval.X(),minval.Y())).getTheta();
    theta[1]=vis.getSick()->worldToLocal(Point(maxval.X(),maxval.Y())).getTheta();
    theta[2]=vis.getSick()->worldToLocal(Point(minval.X(),minval.Y())).getTheta();
    theta[3]=vis.getSick()->worldToLocal(Point(minval.X(),maxval.Y())).getTheta();
    // Make sure they are all in the same "wrap" -- that there is not a 360 deg jump
    // Move the others to same wrap as theta[0]
    for (int k=1;k<4;k++) {
	if (theta[k]>theta[0]+M_PI)
	    theta[k]-=2*M_PI;
	else if (theta[k]<theta[0]-M_PI)
	    theta[k]+=2*M_PI;
    }
    dbg("Leg.update",3) << "Theta = [" << theta[0] << ", "<< theta[1] << ", "<< theta[2] << ", "<< theta[3] << "]" << std::endl;
    
    float mintheta=std::min(std::min(theta[0],theta[1]),std::min(theta[2],theta[3]));
    float maxtheta=std::max(std::max(theta[0],theta[1]),std::max(theta[2],theta[3]));
    std::vector<int> clearsel;
    dbg("Leg.update",3) << "Clear paths for " << mintheta*180/M_PI << ":" << maxtheta*180/M_PI <<  " degrees:   ";
    for (unsigned int i=0;i<vis.getSick()->getNumMeasurements();i++) {
	float angle=vis.getSick()->getAngleRad(i);
	// Wrap to same section as theta[0]
	if (angle>theta[0]+M_PI)
	    angle-=2*M_PI;
	else if (angle<theta[0]-M_PI)
	    angle+=2*M_PI;
	if (angle>=mintheta && angle<=maxtheta) {
	    clearsel.push_back(i);
	    dbgn("Leg.update",3) << i << ",";
	}
    }
    dbgn("Leg.update",3) << std::endl;


    bool useSepLikeLookup=false;
    LookupTable legSepLike;

    if (otherLeg!=NULL) {
	// Calculate separation likelihood using other leg at MLE with computed variance
	if (sqrt(otherLeg->posvar) > ls.getSep()+ls.getSepSigma()) {
	    legSepLike = getLegSepLike(ls.getSep(),ls.getSepSigma(),sqrt(posvar));
	    dbg("Leg.update",3) << "Using simplified model for legsep like since other leg posvar=" << sqrt(otherLeg->posvar) << " > " << ls.getSep()+ls.getSepSigma() << std::endl;
	    // Can compute just using fixed leg separation of ls.getSep() since the spread in possible leg separations is not going to make much difference when the position is poorly determined
	    useSepLikeLookup=true;
	}
    }

    // Compute likelihoods based on composite of apriori, contact measurements, and non-hitting rays
    like.resize(likenx*likeny);

    float apriorisigma=sqrt(posvar+SENSORSIGMA*SENSORSIGMA);
    float stepx=(maxval.X()-minval.X())/(likenx-1);
    float stepy=(maxval.Y()-minval.Y())/(likeny-1);
    for (int ix=0;ix<likenx;ix++) {
	float x=minval.X()+ix*stepx;
	for (int iy=0;iy<likeny;iy++) {
	    float y=minval.Y()+iy*stepy;
	    Point pt(x,y);
	    // float adist=(position-pt).norm();
	    
	    // a priori likelihood
	    // float apriori=normlike(adist,0,apriorisigma);  // WRONG computation!

	    Point delta=position-pt;
	    float apriori=normlike(delta.X(),0,apriorisigma)+normlike(delta.Y(),0,apriorisigma);   // Assume apriorsigma applies in both directions, no covariance

	    // Likelihood with respect to unobstructed paths (leg can't be in these paths)
	    float dclr=1e10;
	    for (unsigned int k=0;k<clearsel.size();k++) {
		Point p1=vis.getSick()->localToWorld(Point(0,0));
		Point p2=vis.getSick()->getWorldPoint(clearsel[k]);
		float dist=segment2pt(p1,p2,pt);
		dclr=std::min(dclr,dist);
		dbg("Leg.update",20) << "p1=" << p1 << ", p2=" << p2 << ", dist[" << k << "] = " << dclr << std::endl;
	    }
	    float clearlike=log(normcdf(log(dclr*2),LOGDIAMMU,LOGDIAMSIGMA));

	    // Likelihood with respect to positive hits
	    float glike=0;
	    float bgsum=0;
	    for (unsigned int k=0;k<fs.size();k++) {
		float dpt=(vis.getSick()->getWorldPoint(fs[k])-pt).norm();
		// Scale it so it is a density per meter in the area of the mean
		float obslike=log(normpdf(log(dpt*2),LOGDIAMMU,LOGDIAMSIGMA)*(UNITSPERM/getDiam()));
		// Take the most likely of the observation being background or this target 
		glike+=std::max(bglike[fs[k]],obslike);
		bgsum+=bglike[fs[k]];
	    }

	    // Likelihood with respect to separation from other leg (if it is set and has low variance)
	    float seplike=0;
	    if (otherLeg!=NULL) {
		// Update likelihood using separation from other leg
		    float d=(otherLeg->position-pt).norm();
		    if (d>MAXLEGSEP*2)
			seplike=-1000;
		    else
			if (useSepLikeLookup)
			seplike=legSepLike.lookup(d);
		    else
			seplike=log(ricepdf(d,ls.getSep(),sqrt(otherLeg->posvar+ls.getSepSigma()*ls.getSepSigma()))*UNITSPERM);
		    if (std::isnan(seplike))
			dbg("Leg.update",3) << "ix=" << ix << ", iy=" << iy << ", d=" << d << ", uselookup=" << useSepLikeLookup << ", sep=" << ls.getSep() << ", sigma=" << (sqrt(otherLeg->posvar+ls.getSepSigma()*ls.getSepSigma()))  << ", seplike=" << seplike << std::endl;
	    }

	    like[ix*likeny+iy]=glike+clearlike+seplike+apriori;
		
	    //assert(isfinite(like[ix*likeny+iy]));
	    dbg("Leg.update",20) << "like[" << ix << "," << iy << "] (x=" << x << ", y=" << y << ") L= " << std::setprecision(1) << like[ix*likeny+iy]  << "  M=" << glike << ",BG=" << bgsum << ", C=" << clearlike << ", A=" << apriori << ", S=" << seplike << std::endl << std::setprecision(3);
	}
    }

    // Find iterator that points to maximum of MLE
    std::vector<float>::iterator mle=std::max_element(like.begin(),like.end());
    maxlike=*mle;
    // Use iterator position to figure out location of MLE
    int pos=distance(like.begin(),mle);
    int ix=pos/likeny;
    int iy=pos-ix*likeny;
    Point mlepos(minval.X()+ix*stepx,minval.Y()+iy*stepy);

    if (maxlike < MINLIKEFORUPDATES) {
	dbg("Leg.update",1) << "Very unlikely placement: MLE position= " << mlepos <<  " with like= " << maxlike << "-- not updating estimates, leaving leg at " << position <<"+=" << sqrt(posvar) <<  std::endl;
	// Don't use this estimate to set the new leg positions, velocities, etc
	return;
    }


    // Find mean location by averaging over grid
    Point sum(0,0);
    double tprob=0;
    int nsum=0;
    for (int ix=0;ix<likenx;ix++) {
	float x=minval.X()+ix*stepx;
	for (int iy=0;iy<likeny;iy++) {
	    float y=minval.Y()+iy*stepy;
	    Point pt(x,y);
	    if (like[ix*likeny+iy]-*mle<-12)   
		// Won't add much!  Less than likenx*likeny*exp(-12)
		continue;
	    double prob=exp(like[ix*likeny+iy]-*mle);
	    if (std::isnan(prob) || !(prob>0))
		dbg("Leg.update",3) << "prob=" << prob << ", like=" << like[ix*likeny+iy] << std::endl;
	    assert(prob>0);
	    sum=sum+pt*prob;
	    tprob+=prob;
	    nsum++;
	    dbg("Leg.updateMLE",3) << "prob=" << prob << ", like=" << like[ix*likeny+iy] << ", pt=" << pt << ", sum=" << sum << std::endl;
	}
    }
    assert(tprob>0);  // Since MLE was found, there must be some point that works
    Point mean=sum/tprob;

    dbg("Leg.update",3) << "Got MLE=" << mlepos << " with like=" << *mle << ", mean position=" << mean << ", tprob=" << tprob << ", nsum=" << nsum << std::endl;
    Point newposition;
    if (USEMLE)
	newposition=mlepos;
    else
	newposition=mean;
    if (velocity.norm() <= STATIONARYVELOCITY ) {
	// Not moving, keep position a little more stable
	dbg("Leg.update",3) << "Speed = " << velocity.norm() << " mm/frame, stabilizing position" << std::endl;
	Point delta=newposition-position;
	position=position+delta/10;
	// TODO: Should keep it in area of likelihood grid that is still possible...
    } else {
	dbg("Leg.update",3) << "Speed = " << velocity.norm() << " mm/frame, not stabilizing position" << std::endl;
	position=newposition;
    }
    dbg("Leg.update",3) << "Target position=" << newposition << ", updated position=" << position << std::endl;

    // Calculate variance (actual mean-square distance from MLE)
    double var=0;
    tprob=0;
    for (int ix=0;ix<likenx;ix++) {
	float x=minval.X()+ix*stepx;
	for (int iy=0;iy<likeny;iy++) {
	    float y=minval.Y()+iy*stepy;
	    Point pt(x,y);
	    if (like[ix*likeny+iy]-*mle<-12)
		// Won't add much!
		continue;
	    double prob=exp(like[ix*likeny+iy]-*mle);
	    if (std::isnan(prob) || !(prob>0))
		dbg("Leg.update",3) << "prob=" << prob << ", like=" << like[ix*likeny+iy] << std::endl;
	    assert(prob>0);
	    var+=prob*pow((pt-position).norm(),2.0);
	    assert(~std::isnan(var));
	    tprob+=prob;
	}
    }
    assert(tprob>0);
    posvar=var/tprob;
    if (posvar< SENSORSIGMA*SENSORSIGMA) {
	dbg("Leg.update",3) << "Calculated posvar for leg is too low (" << sqrt(posvar) << "), setting to " << SENSORSIGMA << std::endl;
	posvar= SENSORSIGMA*SENSORSIGMA;
    }

    dbg("Leg.update",3) << "Leg position= " << position << " +/- " << sqrt(posvar) << " with like= " << *mle << std::endl;
}

void Leg::updateVelocity(int nstep, float fps,Point otherLegVelocity) {
    // Update velocities
    velocity=velocity*(1-1.0f/VELUPDATETC)+getPriorDelta(1)*fps/VELUPDATETC;

    // Reduce speed if over maximum
    float spd=velocity.norm();
    if (spd>MAXLEGSPEED)
	velocity=velocity*(MAXLEGSPEED/spd);
}

void Leg::updateVisibility(const std::vector<float> &bglike) {
    static const float MAXBGLIKEFORVISIBLE=20;
    if (maxlike < MINLIKEFORUPDATES || scanpts.size()==0) {
	dbg("Leg.updateVisiblity",5) << "Leg has maxlike=" << maxlike << ", #scanpts=" << scanpts.size() << "; marking as invisible" << std::endl;
	consecutiveInvisibleCount++;
    } else  {
	for (int i=0;i<scanpts.size();i++)
	    if (bglike[scanpts[i]]< maxlike-MAXBGLIKEFORVISIBLE) {
		dbg("Leg.updateVisiblity",5) << "scan " << scanpts[i] << " has bglike=" << bglike[scanpts[i]] << " < " << MAXBGLIKEFORVISIBLE << " -> leg visible" << std::endl;
		dbg("Leg.updateVisibility",5) << "bglike[23..25]=" << bglike[23] << ", " << bglike[24] << ", " << bglike[25] << std::endl;
		consecutiveInvisibleCount=0;
		return;
	    }
    }
    dbg("Leg.updateVisiblity",5) << "Leg has all " << scanpts.size() << " scanpts with bglike>" << MAXBGLIKEFORVISIBLE << "; marking as invisible" << std::endl;
    consecutiveInvisibleCount++;
}

void Leg::updateDiameterEstimates(const Vis &vis, LegStats &ls)  {
    // Update diameter by looking at direction of position-predictedPosition; if it is away from the current LIDAR, then the current diameter is too large
    // and the corrected diameter would be diam+dot(position-LIDAR,position-predictedPosition)*2
    Point scanDirection=position-vis.getSick()->getOrigin();
    scanDirection=scanDirection/scanDirection.norm();
    float scanError=scanDirection.dot(predictedPosition-position);
    float diamEstimate = getDiam()+scanError;
    diamEstimate=std::max(std::min(diamEstimate,MAXLEGDIAM),MINLEGDIAM);
    updateDiameter(diamEstimate, LEGDIAMSIGMA);
    dbg("Leg.updateDiameterEstimates",3) << "Unit " << vis.getSick()->getId() << ", scan error=" << scanError << " ->  diamestimate=" << diamEstimate << " -> diam=" << getDiam() << std::endl;
    return;
    
    // Update diameter estimate if we have a contiguous set of hits of adequate length
    if (scanpts.size() >= 5 && (scanpts[scanpts.size()-1]-scanpts[0])==(int)(scanpts.size()-1)) {
	dbg("Leg.updateDiameterEstimates",3) << "Updating leg diameter using " << scanpts.size() << " scan points" << std::endl;
	// check that the leg is clearly in the foreground
	const std::vector<unsigned int> srange = vis.getSick()->getRange(0);
	unsigned int firstScan=scanpts[0];
	if (firstScan>0 && srange[firstScan-1]<srange[firstScan]+4*getDiam()) {
	    dbg("Leg.updateDiameterEstimates",3) << "Left edge too close to background: " << srange[firstScan-1] << "<" << srange[firstScan]+4*getDiam() << std::endl;
	    return;
	}
	unsigned int lastScan=scanpts[scanpts.size()-1];
	if (lastScan<vis.getSick()->getNumMeasurements()-1 && srange[lastScan+1]<srange[lastScan]+4*getDiam()) {
	    dbg("Leg.updateDiameterEstimates",3) << "Right edge too close to background: " << srange[lastScan+1] << "<" << srange[lastScan]+4*getDiam() <<  std::endl;
	    return;
	}
	float diamEstimate = (vis.getSick()->getWorldPoint(lastScan)-vis.getSick()->getWorldPoint(firstScan)).norm();
	updateDiameter(diamEstimate,diamEstimate/scanpts.size());   // TODO- improve estimate
    }
}

void Leg::sendMessages(lo_address &addr, int frame, int id, int legnum) const {
    if (lo_send(addr,"/pf/leg","iiiiffffffffi",frame,id,legnum,2,
		position.X()/UNITSPERM,position.Y()/UNITSPERM,
		sqrt(posvar)/UNITSPERM,sqrt(posvar)/UNITSPERM,
		velocity.norm()/UNITSPERM,0.0f,
		velocity.getTheta()*180.0/M_PI,0.0f,
		consecutiveInvisibleCount)<0)
	std::cerr << "Failed send of /pf/leg to " << lo_address_get_url(addr) << std::endl;
}

void Leg::updateDiameter(float newDiam, float newDiamSEM) {
    if (updateDiam) {
	// TODO: track diamSigma
	diam = diam*(1-1/LEGDIAMTC) + newDiam/LEGDIAMTC;
	dbg("LegStats.updateDiameter",3) << "newDiam=" << newDiam << ", updated diam=" << diam << ", sigma=" << diamSigma << std::endl;
    }
}
