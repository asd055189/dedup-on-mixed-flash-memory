#include <bits/stdc++.h>
#include "bpt.h"
#include "crc.h"
using namespace std;
struct CacheNode
{
uint32_t sampling;
bool chance, exist;
int ppn=-1, offset;
int chunksize;
};
void WTTF(char * buf, int ppn, int chunksize, int &actualsize);//write to the file
void checkbpt(char *buf, int chunksize, uint32_t sampling, int &ppn, vector<CacheNode>::iterator &pointer,
int &actualsize, vector<CacheNode> &cache);//check the b+ tree
int main() {
string filename, chunksizename;
int writesize = 0, actualsize = 0;
int cachesize = 0;
vector<CacheNode> cache;
cache.resize(500);
vector<CacheNode>::iterator pointer = cache.begin();
cout << "file & chunksize:";
int ppn = 0;
while (cin >> filename >> chunksizename) {

cout << filename << endl << chunksizename; // |
ifstream Data; // |
Data.open(filename, ios::binary); // read files
ifstream Chunk_Size; // |
Chunk_Size.open(chunksizename); // |
if (Data.is_open()) {
if (Chunk_Size.is_open())
{
cout << endl;
string s;
while (getline(Chunk_Size, s)) {
//cout << "=========next chunk=========" << writesize <<" / "<<actualsize<< endl;
int offset;

int chunksize = atoi(s.c_str());

writesize += chunksize; //calculate the write size

char *buf = new char[chunksize];

Data.read(buf, chunksize);
uint32_t sampling;
memcpy(&sampling, buf, sizeof(uint32_t));//sampling for first 4 Bytes
//cout << "sampling : " << sampling << endl;
bool chksampling = false;
if (cachesize != 0) {
vector<CacheNode>::iterator it = cache.begin();
for (; it != cache.end(); it++) {//find the sampling
if (it->ppn == -1)
break;
if (it->sampling == sampling) {
if (it->chunksize == chunksize) {
ifstream rtc;
rtc.open("./output/" + to_string(it->ppn), ios::binary);
char* buf2 = new char[chunksize];
rtc.read(buf2, chunksize);
if (memcmp(buf2, buf, chunksize) == 0) {
//cout << "sampling dedup! nothing happened"<<sampling << endl;
chksampling = true;
break;
}
}
}
}
if (chksampling)
continue;
else {
//cout << "sampling not found!" << endl;
if (cachesize != 500) {
// cout << "cache is not full" << endl;
CacheNode tmp;
tmp.sampling = sampling;
tmp.ppn = ppn;
tmp.chance = false;
tmp.exist = false;
offset = (chunksize % 4096 == 0) ? chunksize / 4096 : (chunksize / 4096) + 1;
tmp.offset = offset;
tmp.chunksize = chunksize;
WTTF(buf, ppn, chunksize, actualsize);
cache[cachesize] = tmp;
cachesize++;
ppn += offset;
}
else {
// cout << "cache is full" << endl;
checkbpt(buf, chunksize, sampling, ppn, pointer, actualsize, cache);
}
}
}

else {//first data
CacheNode tmp;
tmp.sampling = sampling;
tmp.ppn = ppn;
tmp.chance = false;
tmp.exist = false;
offset = (chunksize % 4096 == 0) ? chunksize / 4096 : (chunksize / 4096) + 1;
tmp.offset = offset;
tmp.chunksize = chunksize;
string wfn = to_string(ppn);
//cout << "first data! write to ppn: " << ppn << endl;

WTTF(buf, ppn, chunksize, actualsize);
cache[cachesize] = tmp;
cachesize++;
ppn += offset;

}
}
}
else {
cout << "Chunk_Size is not opened\n";
return 0;
}
}
else {
cout << "Data is not opened\n";
return 0;
}

cout << "written data : " << writesize << " / actual data : " << actualsize << endl;
}

}
void WTTF(char * buf, int ppn, int chunksize, int &actualsize) {
ofstream wf("./output/" + to_string(ppn), ios::out);
wf.write(buf, chunksize);
actualsize += chunksize;
wf.close();
cout << "file writed:" << ppn << endl;
}

void checkbpt(char *buf, int chunksize, uint32_t sampling, int &ppn, vector<CacheNode>::iterator &pointer,
int &actualsize, vector<CacheNode> &cache) {
/*
1.calculate crc & mix with sampling
2.search on the b+ tree
2a. (if is not in the tree => insert to the tree, and allocate new ppn
(and write))
2b. (if is the tree => read to dedup(is not, then write),
and ppn use treenode's)
3.replace cache node

*/
/*step1*/
int offset;

uint32_t crcval = crc32(buf, chunksize);
uint64_t mixkey = sampling;
mixkey = mixkey << 32 | crcval; //mix crc32_val and sampling_val
//cout << "mixkey : " << mixkey << endl;
/*step1*/
offset = (chunksize % 4094 == 0) ? chunksize / 4096 : (chunksize / 4096) + 1;
CacheNode tmp;
//cout << "sampling : " << sampling << endl;
tmp.sampling = sampling;
tmp.chance = false;
tmp.exist = false;
tmp.offset = offset;
tmp.chunksize = chunksize;
Block *srh = search(mixkey);
	/*step 2a*/

	if (srh == nullptr || !srh->is_leaf) {
		Chunk ctmp;
		tmp.ppn = ppn;
		//cout << "b+ tree not found! write to ppn: " << ppn << endl;
		WTTF(buf, ppn, chunksize, actualsize);
		insert(mixkey, ppn, offset, chunksize);
		ppn += offset;
		//cout << "mixkey : " << mixkey << endl;
	}
	/*step 2b*/
	else {
		bool done=false;
		while(srh->chunk[0].key<=mixkey){
			for (int i = 0; i < MAX_DEGREE; i++) {
				if (srh->chunk[i].key == mixkey) {
						ifstream wtc;
						wtc.open("./output/" + to_string(srh->chunk[i].ppn), ios::binary);
						char *buf2 = new char[chunksize];
						wtc.read(buf2, chunksize);
					/*if (memcmp(buf2, buf, srh->chunk[i].chunksize) != 0) {
						//cout << "mixkey collision! write to ppn: " << ppn << endl;
						done=true;
						tmp.ppn = ppn;
						WTTF(buf, ppn, chunksize, actualsize);
						insert(mixkey, ppn, offset, chunksize);
						ppn += offset;
						break;
					}
					else*/ if (memcmp(buf2, buf, srh->chunk[i].chunksize) == 0) {
						done=true;
						tmp.ppn = srh->chunk[i].ppn;
						tmp.exist = true;
						//cout << "dedup from b+ tree! ppn&exist bit changed " << endl;
						break;
					}
				}
			}
			if(srh->next!=nullptr && !done){
				//cout <<srh->chunk[0].key<<" ~ "<<srh->chunk[srh->size-1].key<<endl<<mixkey<<endl<<endl;
				srh=srh->next;
			}
			else
				break;
		}
		if (!done){
			//cout <<srh->chunk[0].key<<" ~ "<<srh->chunk[srh->size-1].key<<endl<<mixkey<<endl<<endl;
			tmp.ppn = ppn;
			cout << "b+ tree not found! write to ppn: " << ppn << endl;
			WTTF(buf, ppn, chunksize, actualsize);
			insert(mixkey, ppn, offset, chunksize);
			ppn += offset;
			//cout << "mixkey : " << mixkey << endl;
		}
	}

	
	/*step 3*/
	while (pointer->chance) {//search the node can be replaced
			pointer->chance = false;
			pointer++;
			if (pointer == cache.end())
				pointer = cache.begin();
		}
	if (!(pointer->exist)) {//if the node to be replaced is not in the b+ tree
		ifstream wtc;
		wtc.open("./output/" + to_string(pointer->ppn), ios::binary);
		char *buf2 = new char[pointer->chunksize];
		wtc.read(buf2, pointer->chunksize);
		uint32_t crcval = crc32(buf2, pointer->chunksize);
		uint64_t mixkey = pointer->sampling;
		mixkey = mixkey << 32 | crcval; //mix crc32_val and sampling_val
		insert(mixkey, pointer->ppn, pointer->offset, pointer->chunksize);
		//cout << "trans a node to b+ tree!" << endl;
		//cout <<mixkey<<endl;
	}
	//replace the node
	//cout << "replace a node to cache!" << endl;
	*pointer = tmp;
	pointer++;
	if (pointer == cache.end())
		pointer = cache.begin();
	//}
}
