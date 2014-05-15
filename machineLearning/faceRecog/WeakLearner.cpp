#include "WeakLearner.h"


WeakLearner::WeakLearner(double (*haarArg) (Grid &, int, int, int, int), int p_, double rmin_, double rmax_, double cmin_, double cmax_) {
	haar = haarArg;
	p = p_;
	rmin = rmin_;
	rmax = rmax_;
	cmin = cmin_;
	cmax = cmax_;
	cut = 0;
	weight = 0;

}
		
double WeakLearner::trainOnImgs(Grid *faces, int nfaces, Grid *nonfaces, int nnonfaces, double *faceWeights, double *nonfaceWeights, vector<double> cuts) {
	double faceError;
	double nonfaceError;
	int idxErrMin = -1;
	double errMin = INT_MAX; //meh
	for (unsigned int i=0, ii=cuts.size(); i<ii; i++) {
		double cutCur = cuts[i];
		faceError = 0;
		nonfaceError = 0;
		for (int j=0; j<nfaces; j++) {
			if (!evalImgTrain(faces[j], cutCur)) {
				faceError += faceWeights[j];
			}
		}
		for (int j=0; j<nnonfaces; j++) {
			if (evalImgTrain(nonfaces[j], cutCur)) {
				nonfaceError += nonfaceWeights[j];	
			}
		}
		if (faceError + nonfaceError < errMin) {
			idxErrMin = i;
			errMin = faceError + nonfaceError;
		}
	}
	cut = cuts[idxErrMin];
	return errMin;
}

pair<vector<double>, vector<double> > WeakLearner::yieldErrors(Grid *faces, int nfaces, Grid *nonfaces, int nnonfaces) {
	vector<int> faceErrors;
	vector<int> nonfaceErrors;
	faceErrors.reserve(nfaces);
	nonfaceErrors.reserve(nnonfaces);
	for (int i=0; i<nfaces; i++) {
		if (!evalImgTrain(faces[i], cut)) {
			faceErrors.push_back(1);
		} else {
			faceErrors.push_back(0);
		}
	}
	for (int i=0; i<nnonfaces; i++) {
		if (evalImgTrain(nonfaces[i], cut)) {
			nonfaceErrors.push_back(1);
		} else {
			nonfaceErrors.push_back(0);
		}
	}
	return make_pair(faceErrors, nonfaceErrors);
}

bool WeakLearner::evalImgTrain(Grid &img, double curCut) {
	double nr = img.nr;
	double nc = img.nc;
	int rImin = nr * rmin + .5; //.5 to round
	int rImax = nr * rmax + .5;
	int cImin = nc * cmin + .5;
	int cImax = nc * cmax + .5;
	double normFact = (rImax - rImin) * (cImax - cImax);
	return p * haar(img, rImin, rImax, cImin, cImax) / normFact < p * curCut;
}


bool WeakLearner::evalImg(Grid &img, int winRow, int winCol, int dWinRow, int dWinCol) {
	int rImin = winRow + dWinRow * rmin + .5;
	int rImax = winRow + dWinRow * rmax + .5;
	int cImin = winCol + dWinCol * cmin + .5;
	int cImax = winCol + dWinCol * cmax + .5;
	double normFact = (rImax - rImin) * (cImax - cImin);
	return p * haar(img, rImin, rImax, cImin, cImax) / normFact < p * cut;
}
