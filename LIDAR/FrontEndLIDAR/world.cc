#include <algorithm>

#include "lo/lo.h"
#include "world.h"
#include "vis.h"
#include "dbg.h"
#include "parameters.h"

World::World(): groups(GROUPDIST,UNGROUPDIST) {
    lastframe=0;
    nextid=1;
    initWindow();
}

void World::makeAssignments(const Vis &vis, float entrylike) {
    dbg("World.makeAssignments",3) << "makeAssignments(entrylike= " << entrylike << ")" << std::endl;
    // Calculate likelihoods of each scan belonging to each track and assign to highest likelihood
    const SickIO *sick=vis.getSick();
    bestlike.resize(sick->getNumMeasurements());
    assignments.resize(sick->getNumMeasurements());
    legassigned.resize(sick->getNumMeasurements());

    int nassigned=0;
    int nentries=0;
    int nbg=0;
    for (unsigned int f=0;f<sick->getNumMeasurements();f++) {
        bestlike[f]=entrylike;
	assignments[f]=-2;
	legassigned[f]=0;
	if (bglike[f] > bestlike[f]) {
	    bestlike[f]=bglike[f];
	    assignments[f]=-1;
	}
	for (unsigned int i=0;i<people.size();i++) {
	    for (int leg=0;leg<2;leg++) {
		// This is a fudge since the DIAMETER is log-normal and the position itself is normal
		float like =people[i].getObsLike(sick->getPoint(f),leg,vis.getSick()->getFrame());
		dbg("World.makeAssignments",20) << "For assigning scan " << f << " to P" << people[i].getID() << "." << leg << ", like=" << like << ", best so far=" << bestlike[f] << ", bglike=" << bglike[f] << ", entrylike=" << entrylike << std::endl;
		if (like>bestlike[f]) {
		    bestlike[f]=like;
		    assignments[f]=i;
		    legassigned[f]=leg;
		}
	    }
	}
	if (assignments[f]>=0) {
	    nassigned++;
	    dbg("World.makeAssignments",5) << "Assigned scan " << f << " to track " << assignments[f] << "." << legassigned[f] << std::endl;
	} else if (assignments[f]==-2)
	    nentries++;
	else
	    nbg++;
    }

    dbg("World.makeAssignments",3) << "Assigned " << nassigned << " points to targets,  " << nbg  << " to background, and " << nentries << " to entries." << std::endl;
    if (nbg < nassigned+nentries) {
	dbg("World.makeAssignments",2) << "Only have " << nbg*1.0/(nbg+nassigned+nentries)*100 << "% of points assigned to background, using all points for update." << std::endl;
	bg.update(*sick,assignments,true);
    } else {
	// Update background only with points assumed to be background
	bg.update(*sick,assignments,true);
    }
}

void World::track( const Vis &vis, int frame, float fps,double elapsed) {
    int nsteps;
    if (lastframe>0)
	nsteps=frame-lastframe;
    else
	nsteps=1;
    lastframe=frame;

    // Update existing tracks with next prediction
    for (unsigned int i=0;i<people.size();i++)
	people[i].predict(nsteps,fps);

    int num_measurements=vis.getSick()->getNumMeasurements();
    float entryprob=1-exp(-ENTRYRATE/60.0*nsteps/fps);
    float entrylike=log(entryprob/num_measurements*10);  // Like that a scan is a new entry (assumes 10 hits on avg)
    dbg("World.track",2) << "Tracking frame " << frame << std::endl;

    // Calculate background likelihoods
    bglike=bg.like(*vis.getSick());

    // Map scans to tracks
    makeAssignments(vis,entrylike);
    std::vector<int> unassigned;
    for (unsigned int i=0;i<assignments.size();i++)
	if (assignments[i]==-2)
	    unassigned.push_back(i);

    if (unassigned.size()>0 && frame>BGINITFRAMES) {
	dbg("World.track",2) << "Have " << unassigned.size() << " scan points unassigned." << std::endl;
	if ((int)unassigned.size()<MINCREATEHITS) {
	    dbg("World.track",3) <<  "'Need at least " << MINCREATEHITS << " points to assign a new track -- skipping" << std::endl;
	} else {
	    // Create a new track if we can find 2 points that are separated by ~meanlegsep
	    float bestsep=1e10;
	    int bestindices[2]={0,0};
	    for (std::vector<int>::iterator i=unassigned.begin();i!=unassigned.end();i++) {
		for (std::vector<int>::iterator j=unassigned.begin();j!=unassigned.end();j++) {
		    if (i==j)
			continue;
		    float dist=(vis.getSick()->getPoint(*i)-vis.getSick()->getPoint(*j)).norm();
		    if (fabs(dist-MEANLEGSEP)<fabs(bestsep-MEANLEGSEP) ) {
			bestsep=dist;
			dbg("World.track",4) << "Unassigned points " << *i << " and " << *j << " are best separation so far at " << bestsep << std::endl;
			bestindices[0]=*i;
			bestindices[1]=*j;
		    }
		}
	    }
	    if (fabs(bestsep-MEANLEGSEP)<LEGSEPSIGMA*2 ) {
		dbg("World.track",1) << "Creating an initial track using scans " << bestindices[0] << "," << bestindices[1] << " with separation " << bestsep << std::endl;
		Point l1=vis.getSick()->getPoint(bestindices[0]);
		Point l2=vis.getSick()->getPoint(bestindices[1]);
		// Move the points out by legdiam/2 so they make sense
		l1=l1*((l1.norm()+INITLEGDIAM/2)/l1.norm());
		l2=l2*((l2.norm()+INITLEGDIAM/2)/l2.norm());
		Person newPerson = Person(nextid,l1,l2);
		people.push_back(newPerson);
		dbg("World.track",1) << "New person: " << newPerson << std::endl;
		nextid++;
		entrylike=entrylike+log(100);   // Lot more likely that other hits are an entry
		makeAssignments(vis,entrylike);// Redo after adding new tracks, but only do twice (allowing only 1 new person per frame) to limit cpu
	    } else {
		dbg("World.track",2) << "Not creating a track - no pair appropriately spaced: best separation=" << bestsep << std::endl;
	    }
	}
    }

    // Implement assignment
    for (unsigned int p=0;p<people.size();p++) {
	// Build list of points assigned to this person
	std::vector<int> fs[2];
	for (unsigned int j=0;j<assignments.size();j++)
	    if (assignments[j]==(int)p)
		fs[legassigned[j]].push_back(j);

	// Reorganize split between two legs if needed (e.g. frame 958)
	// TODO -- these splits don't take into account which prior position is closer so they result in leg swaps sometimes (e.g. frame 958 of *2923.ferec)
	bool split=false;
	for (int f=0;f<2;f++) {
	    if (fs[f].size()>0 && fs[1-f].size()==0) {
		// One empty, maybe we can move some over at a gap
		if (fs[f].back()-fs[f].front()+1 != (int)fs[f].size()) {
		    // Has a gap
		    for (unsigned int i=1;i<fs[f].size();i++) 
			if (fs[f][i]-fs[f][i-1] != 1) {
			    for (unsigned int j=i;j<fs[f].size();j++)
				fs[1-f].push_back(fs[f][j]);
			    fs[f].resize(i);
			    dbg("World.track",2) << "Splitting preliminary assignment for ID " << people[p].getID() << " at a gap into " << fs[0] << ", " << fs[1] << std::endl;
			    split=true;
			    break;
			}
		}
	    }
	    if (fs[f].size()>0 && fs[1-f].size()==0) {
		// Still one empty, look for a large range jump
		for (unsigned int i=1;i<fs[f].size();i++) { 
		    float r=vis.getSick()->getRange(0)[fs[f][i]];
		    float rprev=vis.getSick()->getRange(0)[fs[f][i-1]];
		    float rjump=abs(r-rprev);
		    if (rjump>INITLEGDIAM/2) {
			for (unsigned int j=i;j<fs[f].size();j++)
			    fs[1-f].push_back(fs[f][j]);
			fs[f].resize(i);
			dbg("World.track",2) << "Splitting preliminary assignment for ID " << people[p].getID() << " at a range jump of " << rjump << " from  " << rprev << " to " << r  << " into " << fs[0] << ", " << fs[1] << std::endl;
			split=true;
			break;
		    }
		}
	    }
	}
	if (split) {
	    // Check if they need to be swapped
	    float swaplike=0;   // Compute difference in likelihood between matching as-is vs. matching swapped
	    for (int f=0;f<2;f++)
		for (int leg=0;leg<2;leg++) {
		    for (unsigned int i=0;i<fs[f].size();i++) {
			float like = people[i].getObsLike(vis.getSick()->getPoint(fs[f][i]),leg,vis.getSick()->getFrame());
			if (leg==f)
			    swaplike+=like;
			else
			    swaplike-=like;
		    }
		}
	    dbg("World.track",2) << "Checking if should swap leg assignments, swaplike=" << swaplike << std::endl;
	    if (swaplike < 0) {
		std::swap(fs[0],fs[1]);
		dbg("World.track",2) << "Swapped assignment for ID " << people[p].getID() << " into " << fs[0] << ", " << fs[1] << std::endl;
	    }
	}

	people[p].update(vis,bglike,fs,nsteps,fps);
    }

    // Delete lost people
    for (unsigned int i=0;i<people.size();i++)
	if (people[i].isDead()) {
	    people.erase(people.begin()+i);
	    i--;
	}

    // Track groups
    groups.update(people,elapsed);

    if (DebugCheck("World.track",2) && people.size() > 0) {
	dbg("World.track",2)  << "People at end of frame " <<  lastframe << ":" << std::endl;
	for (unsigned int i=0;i<people.size();i++)
	    dbg("World.track",2)  << people[i] << std::endl;
    }

    if (frame%2==0)
	draw();

}
        
void World::sendMessages(Destinations &dests, double elapsed) {
    dbg("World.sendMessages",5) << "ndest=" << dests.size() << std::endl;
    for (int i=0;i<dests.size();i++) {
	char cbuf[10];
	dbg("World.sendMessages",6) << "Sending messages to " << dests.getHost(i) << ":" << dests.getPort(i) << std::endl;
	sprintf(cbuf,"%d",dests.getPort(i));
	lo_address addr = lo_address_new(dests.getHost(i), cbuf);
	if (lo_send(addr,"/pf/frame","i",lastframe) < 0) {
	    std::cerr << "Failed send of /pf/frame to " << lo_address_get_url(addr) << std::endl;
	    dests.setFailed(i);
	    if (dests.getFailCount(i) > 100)
		dests.remove(i);
	    continue;
	}
	dests.setSucceeded(i);
	// Handle entries
	std::set<int>exitids = lastid;
	unsigned int priornpeople=lastid.size();
	unsigned activePeople=0;
	for (std::vector<Person>::iterator p=people.begin();p!=people.end();p++){
	    if (p->getAge() >= AGETHRESHOLD) {
		activePeople++;
		exitids.erase(p->getID());
		if ( lastid.count(p->getID()) == 0)
		    lo_send(addr,"/pf/entry","ifii",lastframe,elapsed,p->getID(),p->getChannel());
		lastid.insert(p->getID());
	    }
	}

	// Handle exits
	for (std::set<int>::iterator p=exitids.begin();p!=exitids.end();p++) {
	    lo_send(addr,"/pf/exit","ifi",lastframe,elapsed,*p);
	    lastid.erase(*p);
	}

	// Current size
	if (activePeople != priornpeople)
	    lo_send(addr,"/pf/set/npeople","i",activePeople);

	// Updates
	for (std::vector<Person>::iterator p=people.begin();p!=people.end();p++){
	    if (p->getAge() >= AGETHRESHOLD)
		p->sendMessages(addr,lastframe,elapsed);
	}
	// Groups
	groups.sendMessages(addr,lastframe,elapsed);

	// Geo
	Point center;  // Center of all participants
	for (std::vector<Person>::iterator p=people.begin();p!=people.end();p++)
	    center=center+p->getPosition();
	center=center/people.size();
	for (std::vector<Person>::iterator p=people.begin();p!=people.end();p++) {
	    float centerDist=(p->getPosition()-center).norm();
	    float otherDist=-1;
	    for (std::vector<Person>::iterator p2=people.begin();p2!=people.end();p2++) {
		float dist = (p2->getPosition()-p->getPosition()).norm();
		if (dist>0.001 && (dist<otherDist || otherDist==-1))
		    otherDist=dist;
	    }
	    float exitDist=std::max(0.0f,MAXRANGE-p->getPosition().norm());   // Distance to be out of range
	    if (p->getPosition().Y() < exitDist)
		exitDist=std::max(0.0f,p->getPosition().Y());  // Distance to pass behind sensor

	    if (otherDist>0)
		otherDist/=UNITSPERM;
	    if (lo_send(addr, "/pf/geo","iifff",lastframe,p->getID(),centerDist/UNITSPERM,otherDist,exitDist/UNITSPERM) < 0) {
		std::cerr << "Failed send of /pf/geo to " << lo_address_get_url(addr) << std::endl;
		return;
	    }
	}

	// Done!
	lo_address_free(addr);
    }
}

mxArray *World::convertToMX() const {
    const char *fieldnames[]={"tracks","nextid","npeople","assignments","bglike","bestlike"};
    mxArray *world = mxCreateStructMatrix(1,1,sizeof(fieldnames)/sizeof(fieldnames[0]),fieldnames);

    mxArray *pNextid = mxCreateDoubleMatrix(1,1,mxREAL);
    *mxGetPr(pNextid) = nextid;
    mxSetField(world,0,"nextid",pNextid);

    mxArray *pNpeople = mxCreateNumericMatrix(1,1,mxUINT32_CLASS,mxREAL);
    *(int *)mxGetPr(pNpeople) = people.size();
    mxSetField(world,0,"npeople",pNpeople);

    mxArray *pAssignments = mxCreateNumericMatrix(assignments.size(),2,mxINT32_CLASS,mxREAL);
    int *idata = (int *)mxGetPr(pAssignments);
    assert(legassigned.size()==assignments.size());
    for (unsigned int i=0;i<assignments.size();i++) 
	*idata++=assignments[i];
    for (unsigned int i=0;i<assignments.size();i++) 
	*idata++=legassigned[i];
    mxSetField(world,0,"assignments",pAssignments);

    mxArray *pBestlike = mxCreateDoubleMatrix(bestlike.size(),2,mxREAL);
    double *data = mxGetPr(pBestlike);
    for (unsigned int i=0;i<bestlike.size();i++)
	*data++=bestlike[i];
    mxSetField(world,0,"bestlike",pBestlike);

    mxArray *pBglike = mxCreateDoubleMatrix(1,bglike.size(),mxREAL);
    data=mxGetPr(pBglike);
    for (unsigned int i=0;i<bglike.size();i++)
	*data++=bglike[i];
    mxSetField(world,0,"bglike",pBglike);

    const char *pfieldnames[]={"id","position","legs","legsmeas","prevlegs","legvelocity","scanpts","posvar","prevposvar","velocity","legdiam","leftness","maxlike","like","minval","maxval","age","consecutiveInvisibleCount","totalVisibleCount"};
    mxArray *pPeople;
    if ((pPeople = mxCreateStructMatrix(1,people.size(),sizeof(pfieldnames)/sizeof(pfieldnames[0]),pfieldnames)) == NULL) {
	fprintf(stderr,"Unable to create people matrix of size (1,%ld)\n",people.size());
    }

    for (unsigned int i=0;i<people.size();i++) {
	people[i].addToMX(pPeople,i);
    }

    if (people.size()>0 && mxSetClassName(pPeople,"Person")) {
	fprintf(stderr,"Unable to convert people to a Matlab class\n");
    }

    mxSetField(world,0,"tracks",pPeople);					      

    if (mxSetClassName(world,"World")) {
	fprintf(stderr,"Unable to convert world to a Matlab class\n");
    }

    return world;
}

