// blockInvPP.cpp : Defines the entry point for the console application.
//
#include "stdafx.h"
#include "Matrix.h"
#include <math.h>
#include <stdio.h>
#include <windows.h> 
#include <WinBase.h>
#include <tchar.h>
#include <strsafe.h>
#define numProc 4
using namespace std;

struct ClimbParam {
	Matrix **pfxs;
	int xsSize;
	int i;
	int k;
};

struct SolveYParam {
	SplitMatrix **UVs;
	SplitMatrix **MHs;
	Matrix **zs;
	Matrix **ys;
	int i;
};

int getTime() {
	SYSTEMTIME time;
	GetSystemTime(&time);
	return 60000 * time.wMinute + 1000 * time.wSecond + time.wMilliseconds;
}

Matrix forwardSubCol(Matrix &coefs, Matrix &eqls) {
	Matrix xs = Matrix(eqls.rows.size(), 1);
	for (int y=0; y<coefs.rows.size(); y++) {
		double sum = 0;
		for (int x=0; x<y; x++) {
			sum += coefs.rows[y][x] * xs.rows[x][0];

		}
		xs.rows[y][0] = eqls.rows[y][0] -sum / coefs.rows[y][y];
	}
	return xs;
}

Matrix forwardSub(Matrix &coefs, Matrix &eqls) {
	Matrix xs = Matrix(coefs.rows.size(), 0);
	for (int x=0; x<eqls.rows[0].size(); x++) {
		Matrix eqlsCol = eqls.sliceCol(x);
		Matrix xsCol = forwardSubCol(coefs, eqlsCol);
		xs.appendCol(xsCol);
	}
	return xs;

}

void sliceLs(Matrix *ls, Matrix coefs, int blockSize) {
	int timeBefore = getTime();
	for (int i=0; i<(int)coefs.rows.size() / blockSize; i++) {
		ls[i] = coefs.sliceBlock(i * blockSize, i * blockSize, blockSize, blockSize);
	}
	printf("Spent %d in sliceLs\n", getTime() - timeBefore);
}

void sliceRs(Matrix *rs, Matrix coefs, int blockSize) {
	int timeBefore = getTime();
	for (int i=1; i<(int)coefs.rows.size() / blockSize; i++) {
		rs[i-1] = coefs.sliceBlock(i * blockSize, (i - 1) * blockSize, blockSize, blockSize);
	}
	printf("Spent %d in sliceRs\n", getTime() - timeBefore);

}

void calcGs(Matrix *gs, Matrix *rs, Matrix *ls, int ii) {
	int timeBefore = getTime();
	for (int i=0; i<ii; i++) {
		gs[i] = forwardSub(ls[i+1], rs[i]);
	}
	printf("Spend %d in sliceGs\n", getTime() - timeBefore);


}

void calcBis(Matrix *bis, Matrix *ls, vector<Matrix> &ansBlocks) {
	int timeBefore = getTime();
	for (int i=0; i<ansBlocks.size(); i++) {
		bis[i] = forwardSub(ls[i], ansBlocks[i]);
	}
	printf("Spent %d in sliceB\n", getTime() - timeBefore);

}

void makeGHats(Matrix *gHats, Matrix *gs, int gsSize, int blockSize, int bandwidth) {
	int timeBefore = getTime();

	int firstZero = blockSize - bandwidth;
	for (int i=0; i<gsSize; i++) {
		Matrix gHat = Matrix(blockSize, bandwidth);
		for (int y=0; y<blockSize; y++) {
			for (int x=0; x<bandwidth; x++) {
				gHat.rows[y][x] = gs[i].rows[y][firstZero + x];
			}

		}
		gHats[i] = gHat;
	}
	printf("Spent %d in slicegHats\n", getTime() - timeBefore);
}

void splitByBand(SplitMatrix *split, Matrix *mtx, int mtxSize, int blockSize, int bandwidth) {
	int timeBefore = getTime();

	for (int i=0; i<mtxSize; i++) {
        Matrix top = mtx[i].sliceRows(0, blockSize - bandwidth);
        Matrix bot = mtx[i].sliceRows(blockSize - bandwidth, blockSize);
		split[i] = SplitMatrix(top, bot); 
	}
	printf("Spent %d in splitByBand\n", getTime() - timeBefore);
}

vector<Matrix> solveZsBlockInv(vector<SplitMatrix> &UVs, vector<SplitMatrix> &MHs) {
	vector<Matrix> zs;
	zs.push_back(UVs[0].bottom);
	for (int i=1; i<UVs.size(); i++) {
		zs.push_back(UVs[i].bottom - MHs[i-1].bottom * zs[i-1]);
	}
	return zs;
}

void assemblePrefixComponents(Matrix *comps, SplitMatrix *MHs, SplitMatrix *UVs, int MHsSize) {
	Matrix first = Matrix(UVs[0].bottom.rows.size() + 1, MHs[0].bottom.rows[0].size() + UVs[0].bottom.rows[0].size());
	first.populateCol(first.rows[0].size() - 1, 1);
	first.pasteIn(UVs[0].bottom, 0, MHs[0].bottom.rows[0].size());
	comps[0] = first;
	for (int i=0; i<MHsSize; i++) {
		Matrix compNew = Matrix(UVs[0].bottom.rows.size() + 1, MHs[0].bottom.rows[0].size() + UVs[0].bottom.rows[0].size());
		compNew.populateCol(compNew.rows[0].size() - 1, 1);
		compNew.pasteIn(MHs[i].bottom * -1, 0, 0);
		compNew.pasteIn(UVs[i + 1].bottom, 0, MHs[0].bottom.rows[0].size());
		comps[i+1] = compNew;
	}

}

DWORD WINAPI climbUp(LPVOID lpParam) {
	ClimbParam param = *((ClimbParam*) lpParam);
	if (param.i + param.k < param.xsSize) {
		(*param.pfxs)[param.i+param.k] = (*param.pfxs)[param.i+param.k] * (*param.pfxs)[param.i]; 
	}
	return 0;
}


vector<Matrix> solvePrefixSerial(vector<Matrix> &xs) {

    int n = xs.size();
    int numLevels = (int) (log((double) xs.size()) / log(2.) + .5); 
    for (int i=0; i<numLevels; i++) {
        int stepSize = (unsigned int) pow(2., i + 1);
        int lookForward = (unsigned int) pow(2., i);
        int start = pow(2., i) - 1;
        for (int j=start; j<xs.size(); j+=stepSize) {
            xs[j+lookForward] = xs[j+lookForward] * xs[j];
        }
    }

    for (int k = (unsigned int) pow(2.,n-1.); k>0; k/=2) {
        for (int i=k-1; i<n-1; i+=k) {
            xs[i+k] = xs[i+k] * xs[i];
        }
    }
        return xs;
}



void solvePrefix(Matrix *xs, int xsSize) {
	const int numThreads = numProc / 2;
    int n = numProc;
    int numLevels = (int) (log((double) n) / log(2.) + .5); 
    for (int i=0; i<numLevels; i++) {
        int stepSize = (unsigned int) pow(2., i + 1);
        int lookForward = (unsigned int) pow(2., i);
        int start = pow(2., i) - 1;
		int numSteps = (n - start) / stepSize;

		HANDLE threadHandles[numThreads];
		ClimbParam upParams[numThreads];
		int threadStart = start;
		ClimbParam params[numThreads];
		for (int i=0; i<sizeof(threadHandles) / sizeof(int); i++) {
			ClimbParam next;
			next.i = threadStart;
			next.k = lookForward;
			next.xsSize = xsSize;
			next.pfxs = &xs;
			params[i] = next;
			threadStart += stepSize;
			HANDLE nextHandle = CreateThread(NULL, 0, climbUp, &params[i], 0, NULL);
			threadHandles[i] = nextHandle;
		}
		WaitForMultipleObjects(numThreads, threadHandles, TRUE, INFINITE);
		for (int i=0; i<sizeof(threadHandles) / sizeof(int); i++) {
			CloseHandle(threadHandles[i]);
		}
	}
    for (int k = n/2; k>1; k/=2) {
		HANDLE threadHandles[numThreads];
		ClimbParam params[numThreads];
		for (int t=0, i=k-1; t<sizeof(threadHandles) / sizeof(int); t++, i+=k) {
			ClimbParam next;
			next.i = i;
			next.k = (k + 1) / 2;
			next.xsSize = xsSize;
			next.pfxs = &xs;
			params[t] = next;
			HANDLE nextHandle = CreateThread(NULL, 0, climbUp, &params[t], 0, NULL);
			threadHandles[t] = nextHandle;
		}
		WaitForMultipleObjects(numThreads, threadHandles, TRUE, INFINITE);
		for (int i=0; i<sizeof(threadHandles) / sizeof(int); i++) {
			CloseHandle(threadHandles[i]);
		}
	}
}

void solveZsPrefix(Matrix *zs, SplitMatrix *MHs, int sizeMHs, SplitMatrix *UVs, void (*pfxFunc)(Matrix *, int)) {
	Matrix prfxs[numProc];
	assemblePrefixComponents(prfxs, MHs, UVs, sizeMHs);

	pfxFunc(prfxs, numProc);

	for (int i=0; i<numProc; i++) {
		zs[i] = prfxs[i].sliceBlock(0, prfxs[0].rows[0].size()-1, prfxs[0].rows.size() - 1, 1);
	}
}

vector<Matrix> solveYsSerial(vector<SplitMatrix> &UVs, vector<SplitMatrix> &MHs, vector<Matrix> &zs) {
        vector<Matrix> ys;
        ys.push_back(UVs[0].top);
        for (int i=1; i<UVs.size(); i++) {
                ys.push_back(UVs[i].top - MHs[i-1].top * zs[i-1]);
        }
        return ys;
}

DWORD WINAPI firstY (LPVOID lpParam) {
	SolveYParam first = *((SolveYParam*) lpParam);
	(*first.ys)[0] = (*first.UVs)[0].top;
	return 0;
}

DWORD WINAPI restYs (LPVOID lpParam) {
	SolveYParam param = *((SolveYParam*) lpParam);
	(*param.ys)[param.i] = (*param.UVs)[param.i].top - (*param.MHs)[param.i-1].top * (*param.zs)[param.i-1];
	return 0;
}

void solveYs(Matrix *ys, SplitMatrix *UVs, int UVsSize, SplitMatrix *MHs, Matrix *zs) {


	HANDLE yHandles[numProc];
	SolveYParam params[numProc];
	SolveYParam firstParam;
	firstParam.i = 0;
	firstParam.UVs = &UVs;
	firstParam.ys = &ys;
	params[0] = firstParam;
	yHandles[0] = CreateThread(NULL, 0, firstY, &params[0], 0, NULL);
	for (int i=1; i<UVsSize; i++) {
		SolveYParam next;
		next.i = i;
		next.ys = &ys;
		next.UVs = &UVs;
		next.MHs = &MHs;
		next.zs = &zs;
		params[i] = next;
		yHandles[i] = CreateThread(NULL, 0, restYs, &params[i], 0, NULL);
	}
	WaitForMultipleObjects(numProc, yHandles, TRUE, INFINITE);
	for (int i=0; i<sizeof(yHandles) / sizeof(int); i++) {
		CloseHandle(yHandles[i]);
	}
}

Matrix assembleXs(Matrix *ys, Matrix *zs, int size) {
	Matrix xs = Matrix(size * ys[0].rows.size() + size * zs[0].rows.size(), 1);
	int idx = 0;
	for (int i=0; i<size; i++) {
		for (int j=0; j<ys[i].rows.size(); j++) {
			xs.rows[idx][0] = ys[i].rows[j][0];
			idx++;
		}
		for (int k=0; k<zs[i].rows.size(); k++) {
			xs.rows[idx][0] = zs[i].rows[k][0];
			idx++;
		}
	}
	return xs;
}

Matrix solveXs(Matrix *ls, Matrix *bis, vector<Matrix> &ans, Matrix *gs, int bandwidth, void (*pfxFunc)(Matrix *, int), void (*ysFunc)(Matrix *, SplitMatrix *, int, SplitMatrix *, Matrix *)) {
	int blockSize = ls[0].rows.size();
	Matrix gHats[numProc - 1];
	SplitMatrix MHs[numProc - 1];
	SplitMatrix UVs[numProc];
	makeGHats(gHats, gs, numProc - 1, blockSize, bandwidth);
	splitByBand(MHs, gHats, sizeof(gHats)/sizeof(Matrix), blockSize, bandwidth);
	splitByBand(UVs, bis, numProc, blockSize, bandwidth);
	int timeBefore = getTime();
	Matrix zs[numProc];
	Matrix ys[numProc];
	solveZsPrefix(zs, MHs, sizeof(MHs) / sizeof(SplitMatrix), UVs, pfxFunc);
    
	ysFunc(ys, UVs, sizeof(UVs)/sizeof(SplitMatrix), MHs, zs);
	Matrix xs = assembleXs(ys, zs, numProc);
	printf("solving Xs for %d ms\n", getTime() - timeBefore);
	return xs;
}

Matrix makeCoefs(int mtxSize) {
	Matrix coefs = Matrix(mtxSize, mtxSize);
	coefs.populateDiagonal(0, 0, 1);
	coefs.populateDiagonal(1, 0, -1);
	return coefs;
}

int timeSolveFS(int mtxSize) {
	int blockSize = mtxSize / numProc;
	Matrix coefs = makeCoefs(mtxSize);
	Matrix ans = Matrix(mtxSize, 1);
	ans.populateCol(0, 1);
	int timeBefore = getTime();
	Matrix xs = forwardSub(coefs, ans);
	int dt = getTime() - timeBefore;
	return dt;

}


int timeSolvePP(int mtxSize, void (*pfxFunc)(Matrix *, int), void (*ysFunc)(Matrix *, SplitMatrix *, int, SplitMatrix *, Matrix *)) {

	int blockSize = mtxSize / numProc;
	Matrix coefs = makeCoefs(mtxSize);
	Matrix ans = Matrix(mtxSize, 1);
	ans.populateCol(0, 1);
	int timeBefore = getTime();
	Matrix rs[numProc - 1];
	Matrix ls[numProc];
	Matrix gs[numProc - 1];
	Matrix bis[numProc];
	sliceRs(rs, coefs, blockSize);
	sliceLs(ls, coefs, blockSize);
	calcGs(gs, rs, ls, sizeof(gs)/sizeof(Matrix));
	int bandwidth = 2;

	vector<Matrix> ansBlocks = ans.asRowBlocks(blockSize);
	calcBis(bis, ls, ansBlocks);
	Matrix xs = solveXs(ls, bis, ansBlocks, gs, bandwidth, pfxFunc, ysFunc);
	int dt = getTime() - timeBefore;
	return dt;
	
	return 5;
}



int main(int argc, char *argv[])
{
	const int numSteps = 1;
	int timesPPP[numSteps];
	//int timesFS[numSteps];
	int timesPPS[numSteps];
	int sizes[numSteps];
	int matrixSize = pow(2., 4);
	vector<Matrix> (*pfxSerial)(vector<Matrix>&) = solvePrefixSerial;
	void (*pfxPar)(Matrix *, int) = solvePrefix;

	vector<Matrix> (*ysSerial)(vector<SplitMatrix> &, vector<SplitMatrix> &, vector<Matrix> &) = solveYsSerial;
	void (*ysPar)(Matrix *, SplitMatrix *, int, SplitMatrix *, Matrix *) = solveYs;
	for (int i=0; i<sizeof(timesPPP)/sizeof(int); i++) {
		//timesPPS[i] = timeSolvePP(matrixSize, pfxSerial, ysSerial);
		printf("bing");
		timesPPP[i] = timeSolvePP(matrixSize, pfxPar, ysPar);
		//timesFS[i] = timeSolveFS(matrixSize);

		sizes[i] = matrixSize;
		matrixSize *= 2;
	}

	
	return 0;
}

