#include <bits/stdc++.h>
#include "bpt.h"
#include "crc.h"
using namespace std;
struct CacheNode
{
	uint32_t sampling;
	bool chance,exist;
	int ppn,offset;
	int chunksize;
};
void WTTF(char * buf ,int ppn ,int chunksize,int &actualsize);//write to the file
void checkbpt(char *buf,int chunksize,uint32_t sampling,int &ppn,vector<CacheNode>::iterator &pointer,
			  int &actualsize,vector<CacheNode> &cache,vector<CacheNode>::iterator &it);//check the b+ tree
int main() {
	string filename,chunksizename;
	int writesize=0,actualsize=0;
	cout <<"file & chunksize:";
	while(cin>>filename>>chunksizename){
		int ppn=0;
		vector<CacheNode> cache;
		vector<CacheNode>::iterator pointer;

		cout <<filename<<endl<<chunksizename;	// |
		ifstream Data ;							// |
		Data.open(filename,ios::binary);		// read files
		ifstream Chunk_Size ;					// |
		Chunk_Size.open(chunksizename);			// |

		if(Data.is_open()){
			if (Chunk_Size.is_open())
			{
				cout <<endl;
				string s;
				while(getline(Chunk_Size,s)){
					cout <<"=========next chunk========="<<writesize<<endl;
					int offset;

					int chunksize=atoi(s.c_str());

					writesize+=chunksize;		//calculate the write size

					char buf[chunksize];
					Data.read(buf,chunksize);
					uint32_t sampling;
					memcpy(&sampling,buf,sizeof(uint32_t));//sampling for first 4 Bytes
					cout <<"sampling : "<<sampling<<endl;
					vector<CacheNode>::iterator it=cache.begin();
					if(cache.size()!=0){
						
						for(;it!=cache.end();it++)//find the sampling
						{
							if(it->sampling==sampling){
								break;
							}
						}
						if(it!=cache.end()){
							ifstream wtc ;						
							wtc.open("./output/"+to_string(it->ppn),ios::binary);
							char buf2[it->chunksize];
							wtc.read(buf2,it->chunksize);
							if(buf2!=buf){
								cout <<"sampling collision!"<<endl;
								if(cache.size()!=1000){
									cout <<"cache not fill"<<endl;
									CacheNode tmp;
									tmp.sampling=sampling;
									tmp.ppn=ppn;
									tmp.chance=false;
									tmp.exist=false;
									offset=(chunksize%4096==0)?chunksize/4096:(chunksize/4096)+1;
									tmp.offset=offset;
									tmp.chunksize=chunksize;
									string wfn=to_string(ppn);
									cout <<"write to ppn: "<<ppn<<endl;
									WTTF(buf,ppn,chunksize,actualsize);
									cache.push_back(tmp);
									ppn+=offset;
								}
								else{
									cout <<"cache is fill"<<endl;
									checkbpt(buf,chunksize,sampling,ppn,pointer,actualsize,cache,it);
								}
							}
							else{
								cout <<"sampling dedup! nothing happened"<<endl;
								it->chance=true;
								continue;
							}
						}
						else{
							cout <<"sampling not found!"<<endl;
							if(cache.size()!=1000){
								cout <<"cache is not fill"<<endl;
								CacheNode tmp;
								tmp.sampling=sampling;
								tmp.ppn=ppn;
								tmp.chance=false;
								tmp.exist=false;
								offset=(chunksize%4096==0)?chunksize/4096:(chunksize/4096)+1;
								tmp.offset=offset;
								tmp.chunksize=chunksize;
								string wfn=to_string(ppn);
								cout <<"write to ppn: "<<ppn<<endl;
								WTTF(buf,ppn,chunksize,actualsize);
								cache.push_back(tmp);
								ppn+=offset;
							}
							else{
								cout <<"cache is fill"<<endl;
								checkbpt(buf,chunksize,sampling,ppn,pointer,actualsize,cache,it);
							}
						}
					}
					else{
						CacheNode tmp;
						tmp.sampling=sampling;
						tmp.ppn=ppn;
						tmp.chance=false;
						tmp.exist=false;
						offset=(chunksize%4096==0)?chunksize/4096:(chunksize/4096)+1;
						tmp.offset=offset;
						tmp.chunksize=chunksize;
						string wfn=to_string(ppn);
						cout <<"first data! write to ppn: "<<ppn<<endl;
						
						WTTF(buf,ppn,chunksize,actualsize);
						cache.push_back(tmp);
						ppn+=offset;
						pointer=cache.begin();
						
					}
				}
			}
			else{
				cout <<"Chunk_Size is not opened\n";
				return 0;
			}
		}
		else{
			cout <<"Data is not opened\n";
			return 0;
		}

		cout << "written data : "<<writesize<<" / actual data : "<<actualsize<<endl;
	}

}

void WTTF(char * buf ,int ppn ,int chunksize,int &actualsize){
	ofstream wf("./output/"+to_string(ppn),ios::out);
	wf.write(buf,chunksize);
	actualsize+=chunksize;
	wf.close();
	cout <<"file writed:"<<ppn<<endl;
}

void checkbpt(char *buf,int chunksize,uint32_t sampling,int &ppn,vector<CacheNode>::iterator &pointer,
			  int &actualsize,vector<CacheNode> &cache,vector<CacheNode>::iterator &it){
	/*
	1.calculate crc & mix with sampling
	2.search on the b+ tree
		2a. (if is not in the tree => insert to the tree, and allocate new ppn
			(and write))
		2b. (if is the tree 	   =>  read to dedup(is not, then write), 
			and ppn use treenode's)
	3.replace cache node

	*/
	/*step1*/
	int offset;
	uint32_t crcval=crc32(buf,chunksize);
	uint64_t mixkey=sampling;
	mixkey =mixkey<< 32|crcval; //mix crc32_val and sampling_val
	cout <<"mixkey : "<<mixkey<<endl;
	/*step1*/
	offset=(chunksize%4094==0)?chunksize/4096:(chunksize/4096)+1;
	CacheNode tmp;
	cout <<"sampling : "<<sampling<<endl;
	tmp.sampling=sampling;
	tmp.chance=false;
	tmp.exist=false;
	tmp.offset=offset;
	tmp.chunksize=chunksize;
	Block *srh=search(mixkey);
	/*step 2a*/

	if(srh==nullptr || !srh->is_leaf){
		Chunk ctmp;
		tmp.ppn=ppn;
		cout <<"b+ tree not found! write to ppn: "<<ppn<<endl;
		WTTF(buf,ppn,chunksize,actualsize);
		insert(mixkey,ppn,offset,chunksize);
		ppn+=offset;
		cout <<"mixkey : "<<mixkey<<endl;

	}

	/*step 2b*/
	else{
		for(int i=0;i<35;i++){
			if(srh->chunk[i].key==mixkey){
				ifstream wtc ;						
				wtc.open("./output/"+to_string(srh->chunk[i].ppn),ios::binary);
				char buf2[chunksize];
				wtc.read(buf2,chunksize);
				if(buf!=buf2){
					cout <<"mixkey collision! write to ppn: "<<ppn<<endl;
					tmp.ppn=ppn;
					WTTF(buf,ppn,chunksize,actualsize);
					insert(mixkey,ppn,offset,chunksize);
					ppn+=offset;
					break;
				}
				else{
					tmp.ppn=srh->chunk[i].ppn;
					tmp.exist=true;
					cout<<"dedup from b+ tree! ppn&exist bit changed "<<endl;
				}
			}
			else if (i==34){
				tmp.ppn=ppn;
				cout <<"b+ tree not found! write to ppn: "<<ppn<<endl;
				WTTF(buf,ppn,chunksize,actualsize);
				insert(mixkey,ppn,offset,chunksize);
				ppn+=offset;
				cout <<"mixkey : "<<mixkey<<endl;
			}
		}
	}
	/*step 3*/
	/*if(cache.size()!=1000){//if cache is not full yet
		cout << "insert a node to cache!"<<endl;
		cache.push_back(tmp);
	}
	else{*/
		while(pointer->chance){//search the node can be replaced
			pointer->chance=false;
			pointer++;
			if(pointer==cache.end())
				pointer=cache.begin();
		}
		if(!(it->exist)){//if the node to be replaced is not in the b+ tree
			ifstream wtc ;
			wtc.open("./output/"+to_string(it->ppn)	,ios::binary);
			char buf2[it->chunksize];
			wtc.read(buf2,it->chunksize);
			uint32_t crcval=crc32(buf2,it->chunksize);
			uint64_t mixkey=sampling;
			mixkey =mixkey<< 32|crcval; //mix crc32_val and sampling_val
			insert(mixkey,it->ppn,it->offset,it->chunksize);
			cout <<"trans a node to b+ tree!"<<endl;
		}
		//replace the node
		cout <<"replace a node to cache!"<<endl;
		*it=tmp;
	//}
}