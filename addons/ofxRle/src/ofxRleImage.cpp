#include "ofxRleImage.h"

// This adds a 1-pixel vertical offset, which is important for drawing lines on some graphics cards.
bool ofxRleImage::useDrawOffset = false;

// This is used for switching between the fast 8-byte compression technique and slower per-byte technique.
bool ofxRleImage::useFastEncoding = false;

ofxRleImage::ofxRleImage() :
width(0),
height(0) {
}

void ofxRleImage::load(ofImage& img) {
	if(img.type == OF_IMAGE_GRAYSCALE) {
		
		centroid.x = centroid.y = 0;
		
		data.clear();
		width = img.getWidth();
		height = img.getHeight();
		
		unsigned char *raw = img.getPixels();
		int nPixels = width * height;
		
		if(useFastEncoding) {	
			// Scan through the image in 8-bytes chunks. (unsigned long long, or uint64_t). 
			// Achieve additional optimizations by assuming that the image is mostly black.
			
			int bytesPerChunk = sizeof(uint64_t);
			
			uint64_t *firstChunk = (uint64_t*) raw;
			uint64_t *curChunk = &firstChunk[1];
			uint64_t *lastChunk = &firstChunk[(nPixels / bytesPerChunk) - 1];
			
			// Note: I ignore the first 8 and last 8 bytes of the image, for now. 
			// (that's the 1 and -1 above.)
			firstChunk[0] = 0;
			lastChunk[0] = 0;
			
			int pixelIndex = 0;
			unsigned char prevVal = 0;
			while (curChunk < lastChunk){
				uint64_t curChunkVal = *curChunk;
				if (curChunkVal != 0){
					uint64_t nextChunkVal = *(curChunk+1);
					unsigned char *triplet = (unsigned char *) (curChunk - 1);
					
					// We store all 24 bytes but only record edges 
					// in the curr (middle) set of 8 chars. 
					unsigned char prevVal = triplet[bytesPerChunk - 1];
					for (int curIndex = bytesPerChunk; curIndex < (bytesPerChunk + bytesPerChunk + 1); curIndex++){
						unsigned char curVal = triplet[curIndex];
						if (curVal != prevVal) {
							if (curIndex < (bytesPerChunk + bytesPerChunk)){
								data.push_back( ((curChunk - firstChunk - 1) * bytesPerChunk) + curIndex );
							} else if (nextChunkVal == 0){
								data.push_back( ((curChunk - firstChunk - 1) * bytesPerChunk) + curIndex );
							}
						}
						prevVal = curVal;
					}
				}
				curChunk++;
			}
		} else {
			// Slow but simple method. 
			unsigned char curVal;
			unsigned char prevVal = 0;
			for (int curIndex = 0; curIndex < nPixels; curIndex++){
				curVal = raw[curIndex];
				if (curVal != prevVal){
					data.push_back(curIndex);
				}
				prevVal = curVal;
			}
		}
		
		// If there is an odd number of points, the last point must be closed.
		if(data.size() % 2 == 1) {
			data.push_back(nPixels - 1);
		}
	}
}

unsigned int ofxRleImage::getWidth() const {
	return width;
}

unsigned int ofxRleImage::getHeight() const {
	return height;
}

void ofxRleImage::update() {
	// Convert RLE data into lines.
	int nLineEndpoints = data.size();
	lines.clear();
	// a good guess for how much space is needed
	lines.reserve(nLineEndpoints);
	unsigned int prevRow = 0;
	for (int i = 0; i < nLineEndpoints; i++){
		int curIndex = data[i];
		RlePoint2d curPoint = lines[i];
		curPoint.x = (curIndex % width);
		curPoint.y = (curIndex / width);
		
		unsigned int curRow = curPoint.y;

		// with end points
		if(i % 2 == 1) {
			// make sure any inbetween lines have been added
			while(prevRow < curPoint.y) {
				RlePoint2d rowEnd;
				rowEnd.x = width;
				rowEnd.y = prevRow;
				prevRow++;
				lines.push_back(rowEnd);
				
				RlePoint2d rowStart;
				rowStart.x = 0;
				rowStart.y = prevRow;
				lines.push_back(rowStart);
			}
		}
		
		prevRow = curRow;
		
		lines.push_back(curPoint);
	}
}

//========================================================
void ofxRleImage::computeCentroid(){
	
	float x = 0; 
	float y = 0; 
	
	if (lines.size() > 0){
		int n = lines.size();
		for (int i=0; i<n; i++){
			RlePoint2d curPoint = lines[i];
			x += curPoint.x;
			y += curPoint.y;
		}
		centroid.x = x/(float)n;
		centroid.y = y/(float)n;
	}
}


//========================================================
void ofxRleImage::draw(int x, int y) const {
	if(lines.size()) {
		glPushMatrix();
		
		glTranslatef(x, y, 0);
		if(useDrawOffset) {
			glTranslatef(0, 1, 0);
		}

		// Draw lines as a vertex array.
		glEnableClientState(GL_VERTEX_ARRAY);
		glVertexPointer(2, GL_INT, 0, &lines[0]);
		glDrawArrays(GL_LINES, 0, lines.size());
		glDisableClientState(GL_VERTEX_ARRAY);
		
		glPopMatrix();
	}
}

/*
 For more information on overloading << and >> to work with iostream, see:
 http://www.csse.monash.edu.au/~jonmc/CSE2305/Topics/10.19.OpOverload/html/text.html#an_important_use_of_operator_overloading
 */
ostream& operator<<(ostream& out, const ofxRleImage& img) {
	out.write((char*) &(img.width), sizeof(img.width));
	out.write((char*) &(img.height), sizeof(img.height));
	
	unsigned int dataSize = img.data.size();
	out.write((char*) &(dataSize), sizeof(dataSize));
	for(int i = 0; i < dataSize; i++) {
		unsigned int cur = img.data[i];
		out.write((char*) &(cur), sizeof(cur));
	}
	
	return out;
}

void ofxRleImage::save(string filename) const {
	ofstream file;
	file.open(ofToDataPath(filename).c_str(), ios::out | ios::trunc | ios::binary);
	if(file.is_open()) {
		file << *this;
	} else {
		ofLog(OF_LOG_ERROR, "Could not open file " + filename + " for saving.");
	}
	file.close();
}

istream& operator>>(istream& in, ofxRleImage& img) {
	in.read((char*) &(img.width), sizeof(img.width));
	in.read((char*) &(img.height), sizeof(img.height));
	
	unsigned int dataSize;
	in.read((char*) &(dataSize), sizeof(dataSize));
	img.data.resize(dataSize);
	for(int i = 0; i < dataSize; i++) {
		unsigned int& cur = img.data[i];
		in.read((char*) &(cur), sizeof(cur));
	}
	
	img.update();
	
	return in;
}

void ofxRleImage::load(string filename) {
	ifstream file;
	file.open(ofToDataPath(filename).c_str(), ios_base::in | ios::binary);
	if(file.is_open()) {
		file >> *this;
	} else {
		ofLog(OF_LOG_ERROR, "Could not open file " + filename + " for loading.");
	}
	file.close();
}
