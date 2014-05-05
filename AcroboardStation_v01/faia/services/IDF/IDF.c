/*
 * IDF.c
 *
 * Created: 6/21/2013 5:41:07 PM
 *  Author: Administrator
 */ 

#include <asf.h>

#define QUEUE_LEN 288


static const float g_IDF_002YR[] /*PROGMEM*/ = {4.5880268974F,	7.1318375885F,	11.0860525725F,	22.3057535906F,	34.673077437F,	53.8974078625F,	83.78058104500F,	108.4445785453F,	168.5711829573F};
static const float g_IDF_005YR[] /*PROGMEM*/ = {7.9170088664F,	11.9468259532F,	18.0278502607F,	34.6069335435F,	52.222123127F,	78.8035767591F,	118.9151903099F,	151.2742842021F,	228.2740331683F};
static const float g_IDF_010YR[] /*PROGMEM*/ = {8.8020022473F,	13.1868834746F,	19.7561748893F,	37.4938448383F,	56.1721014161F,	84.1552791161F,	126.0787975626F,	159.7123428585F,	239.2760187478F};
static const float g_IDF_025YR[] /*PROGMEM*/ = {9.972736619F,	14.8068150927F,	21.984113445F,	41.1304533015F,	61.0675925759F,	90.6688490808F,	134.6187044038F,	169.6338982393F,	251.8604331638F};
static const float g_IDF_050YR[] /*PROGMEM*/ = {11.6956663359F,	17.1531562025F,	25.1572470739F,	46.1607236264F,	67.700469562F,	99.2911986392F,	145.6229504905F,	182.1882006983F,	267.2017630349F};
//static const float g_IDF_100YR[] /*PROGMEM*/ = {1.0f,2.0f,3.0f,4.0f,5.0f,6.0f,7.0f,8.0f,9.0f};


float g_idf_queue[QUEUE_LEN];
size_t queue_head;

void idf_queue_init();
size_t idf_queue_head();
void idf_queue_tail();
void idf_queue_head_insert(const float val);
void idf_queue_getAt();

void idf_queue_init() {
	for(int i=0;i<QUEUE_LEN;++i) g_idf_queue[i]=0.0f;
}

size_t idf_queue_head() {
	return queue_head;
}

//void idf_queue_tail() {
	//return (queue_head==0)?QUEUE_LEN:queue_head-1;
//}
//

void idf_queue_head_insert(const float val) {
	g_idf_queue[queue_head++] = val;
	if(queue_head==QUEUE_LEN) queue_head=0;
}

void idf_queue_iterate_for(const size_t num,void (*fn) (float)) {
	
	size_t it = queue_head;
	
	for(size_t n=0;n<num;++n) {
		fn(g_idf_queue[it]);
		if(it!=0) {
			--it;
		} else {
			it=QUEUE_LEN-1;
		}
	}
}
