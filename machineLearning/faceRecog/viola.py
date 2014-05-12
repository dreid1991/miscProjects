import Image
import numpy as np
import os
# am using row, column notation like a normal person
def toGreyscale(rgb):
	greyscale = np.zeros(len(rgb))
	for i in range(len(rgb)):
		greyscale[i] = .2989 * rgb[i][0] / 255 + .5870 * rgb[i][1] / 255 + .1140 * rgb[i][2] / 255 # see stackoverflow.com/question/687261/converting-rgb-to-grayscale-intensity.  has to do with human perception
	return greyscale

def to2d(flat, nr, nc):
	img2d = []
	for i in range(len(flat) / nc):
		img2d.append(flat[nc*i : nc*(i+1)])
	return np.array(img2d, np.float64)

def toIntIntensity(as2d):
	intense = np.zeros((as2d.shape[0] + 1, as2d.shape[1] + 1), np.float64)
	#intense has bordering row, col, of zeros
	for nRow in range(0, as2d.shape[0]):
		sumCol = 0
		for nCol in range(0, as2d.shape[1]):
			sumCol += as2d[nRow][nCol]
			intense[nRow+1][nCol+1] = intense[nRow][nCol+1] + sumCol
	return intense

def loadImages(folder, n):
	fns = os.listdir(folder)[:n]
	npImgs = []
	for f in fns:
		img = Image.open(folder + f)
		nc, nr = img.size
		flatRGB = np.array(img.getdata())
		flatGreyscale = toGreyscale(flatRGB)
		as2d = to2d(flatGreyscale, nr, nc)
		npImgs.append(toIntIntensity(as2d))
	return npImgs
class WeakLearner:
	def __init__(haar, p, cut, rmin, rmax, cmin, cmax):

		self.haar = haar
		self.p = p
		self.cut = cut
		self.rmin = rmin
		self.rmax = rmax
		self.cmin = cmin
		self.cmax = cmax
	def eval(img):
		shaper = img.shape[0]
		shapec = img.shape[1]
		rImin = int(round(shaper * rmin))
		rImax = int(round(shaper * rmax))
		cImin = int(round(shapec * cmin))
		cImax = int(round(shapec * cmax))
		normFact = (rImax - rImin) * (cImax - cImin)
		return int(self.p * self.haar(img, self.rmin, self.rmax, self.cmin, self.cmax, rImin, rImax, cImin, cImax) / norm < self.p * self.cut)

def getIntense(img, rmin, rmax, cmin, cmax):
	return img[rmax][cmax] - img[rmin][cmax] - img[rmax][cmin] + img[rmin][cmin]


#for each, do two rotations and both parities.  Except 4.  Only need one rotation and both parities for that. switching parity is equil to rotating
#also, I would like not normalize by size window size, since otherwise a given theta would only apply at one dimension
def haarTwoHoriz(img, rmin, rmax, cmin, cmax, rImin, rImax, cImin, cImax):
	rcut = int(round(img.shape[0] * (rmin + rmax) / 2.))
	iA = getIntense(img, rcut, rImax, cImin, cImax)
	iB = getIntense(img, rIMin, rcut, cImin, cImax)
	return (iA - iB)


def haarTwoVert(img, rmin, rmax, cmin, cmax, rImin, rImax, cImin, cImax):
	ccut = int(round(img.shape[1] * (cmin + cmax) / 2.))
	iA = getIntense(img, rIMin, rImax, ccut, cImax)
	iB = getIntense(img, rIMin, rImax, cImin, ccut)
	return iA - iB

def haarThreeHoriz(img, rmin, rmax, cmin, cmax, rImin, rImax, cImin, cImax):
	rcutLow = int(round(img.shape[0] * (rmin + rmax) / 3.))
	rcutHigh = int(round(img.shape[0] * 2 * (rmin + rmax) / 3.))
	iA = getIntense(img, rIMin, rcutLow, cImin, cImax)
	iB = getIntense(img, rcutLow, rcutHigh, cImin, cImax)
	iC = getIntense(img, rcutHigh, rImax, cImin, cImax)
	return iB - iA - iC

def haarThreeVert(img, rmin, rmax, cmin, cmax, rImin, rImax, cImin, cImax):
	ccutLow = int(round(img.shape[1] * (cmin + cmax) / 3.))
	ccutHigh = int(round(img.shape[1] * 2 * (cmin + cmax) / 3.))
	iA = getIntense(img, rIMin, rImax, cImin, ccutLow)
	iB = getIntense(img, rIMin, rImax, ccutLow, ccutHigh)
	iC = getIntense(img, rIMin, rImax, ccutHigh, cImax)
	return iB - iA - iC

def haarFour(img, rmin, rmax, cmin, cmax, rImin, rImax, cImin, cImax):
	rcut = int(round(img.shape[0] * (rmin + rmax) / 2.))
	ccut = int(round(img.shape[1] * (cmin + cmax) / 2.))
	iA = getIntense(img, rIMin, rcut, cImin, ccut)
	iB = getIntense(img, rcut, rImax, cImin, ccut)
	iC = getIntense(img, rIMin, rcut, ccut, cImax)
	iD = getIntense(img, rcut, rImax, ccut, cImax)
	return (iA + iD) - (iB + iB)

#HEY - The learners will be paramatrized with positions being index values, then in production I'm going to convert them to fractional positions in the window since the window is variable
def sweepCut(rmin, rmax, cmin, cmax, cutmin, cutmax, dcut, func, allLearners):
	cut = cutmin
	while cut <= cutmax:
		allLearners.append(WeakLearner(func, 1, rmin, rmax, cmin, cmax, cut)
		allLearners.append(WeakLearner(func, -1, rmin, rmax, cmin, cmax, cut)
		cut += dcut


def assembleWeaks(nr, nc):
	lns = []
	#make all 2 verticals
	step = 1
	fnr = float(nr)
	fnc = float(nc)
	for numCols in range(2, nc+1, 2 * step):
		for numRows in range(1, nr+1, step):
			for c in range(1, 1 + nc - numCols, step):
				for r in range(1, 1 + nc - numRows, step):
					cMinFrac = c / fnc
					cMaxFrac = (c + numCols) / fnc
					rMinFrac = r / fnr
					rMaxFrac = (r + numRows) / fnr
					sweepCut(rMinFrac, rMaxFrac, cMinFrac, cMaxFrac, -.5, .5, .1, haarTwoVert, lns)

	#horizonal 2's
	for numRows range(2, nr+1, 2 * step):
		for numColsin range(1, nc+1, step):
			for c in range(1, 1 + nc - numCols, step):
				for r in range(1, 1 + nc - numRows, step):
					cMinFrac = c / fnc
					cMaxFrac = (c + numCols) / fnc
					rMinFrac = r / fnr
					rMaxFrac = (r + numRows) / fnr
					sweepCut(rMinFrac, rMaxFrac, cMinFrac, cMaxFrac, -.5, .5, .1, haarTwoHoriz, lns)
#abs of normalized haar can be at MOST 0.5 (1 diff between the groups, then div by total # sqrs). cut must only sweep -.5, .5

imgs = loadImages('faces/', 1)
'''
a = imgs[0][0]
b = imgs[1][0]
rr = 40
cc = 30
s = 0.
for r in range(rr):
	for c in range(cc):
		s += b[r][c]
print s
print a[rr][cc]


'''