#include <mat.h>
#include "sickio.h"
#include "tracker.h"
#include "snapshot.h"

Snapshot::Snapshot() {
}

void Snapshot::append(const SickIO *sick, const Tracker *t) {
    vis.push_back(sick->convertToMX());
    bg.push_back(t->getClassifier()->getBackground()->convertToMX());
    tracker.push_back(t->convertToMX());
}

void Snapshot::save(const char *filename) const {
    printf("Saving snapshot of length %ld  in %s\n", vis.size(), filename);
    MATFile *pmat = matOpen(filename,"w");
    if (pmat==NULL) {
	fprintf(stderr,"Unable to create MATLAB output file %s\n", filename);
	return;
    }
    
    const char *fieldnames[]={"vis","bg","tracker"};
    mxArray *snap = mxCreateStructMatrix(vis.size(),1,sizeof(fieldnames)/sizeof(fieldnames[0]),fieldnames);

    for (int i=0;i<(int)vis.size();i++) {
	mxSetField(snap,i,"vis",vis[i]);
	mxSetField(snap,i,"bg",bg[i]);
	mxSetField(snap,i,"tracker",tracker[i]);
    }
    matPutVariable(pmat, "snap",snap);

    if (matClose(pmat) != 0) 
	fprintf(stderr,"Error closing MATLAB output file %s\n", filename);

    mxDestroyArray(snap);
}

