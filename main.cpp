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

void WTTF(char * buf, int ppn, int chunksize,long long int &actualsize);//write to the file
void checkbpt(char *buf, int chunksize, uint32_t sampling, int &ppn, vector<CacheNode>::iterator &pointer,
			long long int &actualsize, vector<CacheNode> &cache,long long int &readsize,
			long long int &sampling_hit,long long int &bpt_hit,long long int &sampling_collision,long long int &bpt_collision,
			long long int &bpt_read,int &num_of_read_page);//check the b+ tree
int main() {
	string filename, chunksizename;
	long long int writesize = 0, actualsize = 0,readsize=0;
	long long int sampling_hit=0,bpt_hit=0,sampling_collision=0,bpt_collision=0;
	long long int sampling_read=0,bpt_read=0;
	int num_of_read_page=0;
	int cachesize = 0;
	int chunk_num=0;
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
					chunk_num++;
					//cout << "=========next chunk=========" << writesize <<" / "<<actualsize<< endl;
					int offset;

					int chunksize = atoi(s.c_str());

					writesize += chunksize; //calculate the write size

					char *buf = new char[chunksize];
					Data.read(buf, chunksize);
					bitset<32> s;
					uint32_t sampling;
					int shift=0;
					for (int i=0,j=0;j<32;i+=(chunksize/32),j++){
						s.set(j,buf[i]&0x01);
					}
					sampling=s.to_ulong();
					//memcpy(&sampling, buf, sizeof(uint32_t));//sampling for first 4 Bytes
					//cout << "sampling : " << sampling << endl;
					bool chksampling = false;
					if (cachesize != 0) {
						vector<CacheNode>::iterator it = cache.begin();
						for (; it != cache.end(); it++) {//find the sampling
							if (it->ppn == -1)
								break;
							if (it->sampling == sampling) {
								if (it->chunksize == chunksize) {
									sampling_collision++;
									ifstream rtc;
									rtc.open("./output/" + to_string(it->ppn), ios::binary);
									char* buf2 = new char[chunksize];
									rtc.read(buf2, chunksize);
									readsize+=chunksize;
									num_of_read_page+=offset;
									sampling_read+=chunksize;
									if (memcmp(buf2, buf, chunksize) == 0) {

										sampling_hit++;
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
							if (cachesize < 500) {
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
								checkbpt(buf, chunksize, sampling, ppn, pointer, actualsize, cache,readsize,sampling_hit,
									bpt_hit,sampling_collision,bpt_collision,bpt_read,num_of_read_page);
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
		cout <<"sampling_collision : "<<sampling_collision<<"/ sampling_hit : "<<sampling_hit<<endl;
		cout <<"bpt_collision : "<<bpt_collision<<"/ bpt_hit : "<<bpt_hit<<endl;
		cout <<"sampling_read : "<<sampling_read << "/ bpt_read : "<<bpt_read<<endl;
		cout <<"read size : "<<readsize<<"(Bytes)"<<endl;
		cout <<"actual write rate"<<setprecision(5)<<fixed<<double(actualsize)/writesize<<endl;
		cout <<"# of chunk : "<<chunk_num<<endl<<"avg size : "<<(double)writesize/chunk_num<<"Bytes"<<endl;
		cout <<"num_of_read_page : " <<num_of_read_page<<endl;

	}

}
void WTTF(char * buf, int ppn, int chunksize,long long int &actualsize) {
	ofstream wf("./output/" + to_string(ppn), ios::out);
	wf.write(buf, chunksize);
	actualsize += chunksize;
	wf.close();
	//cout << "file writed:" << ppn << endl;
}

void checkbpt(char *buf, int chunksize, uint32_t sampling, int &ppn, vector<CacheNode>::iterator &pointer,
			long long int &actualsize, vector<CacheNode> &cache,long long int &readsize,
			long long int &sampling_hit,long long int &bpt_hit,long long int &sampling_collision,long long int &bpt_collision,
			long long int &bpt_read ,int &num_of_read_page){
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
		tmp.exist=true;
		//cout << "b+ tree not found! write to ppn: " << ppn << endl;
		WTTF(buf, ppn, chunksize, actualsize);
		insert(mixkey, ppn, offset, chunksize);
		tmp.exist=true;
		ppn += offset;
		//cout << "mixkey : " << mixkey << endl;
	}
	/*step 2b*/
	else {
		bool done=false;
		while(srh->chunk[0].key<=mixkey){
			for (int i = 0; i < MAX_DEGREE; i++) {
				if (srh->chunk[i].key == mixkey) {
					bpt_collision++;
					ifstream wtc;
					wtc.open("./output/" + to_string(srh->chunk[i].ppn), ios::binary);
					char *buf2 = new char[chunksize];
					wtc.read(buf2, chunksize);
					readsize+=chunksize;
					num_of_read_page+=srh->chunk[i].offset;
					bpt_read+=chunksize;
					if (memcmp(buf2, buf, srh->chunk[i].chunksize) == 0) {
						bpt_hit++;
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
			//cout << "b+ tree not found! write to ppn: " << ppn << endl;
			WTTF(buf, ppn, chunksize, actualsize);
			insert(mixkey, ppn, offset, chunksize);
			tmp.exist=true;
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
		readsize+=chunksize;
		bpt_read+=chunksize;
		num_of_read_page+=pointer->offset;
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