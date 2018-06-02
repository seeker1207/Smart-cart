#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <raspicam/raspicam_cv.h>
#include <opencv2/imgproc.hpp>
#include "/usr/include/mysql/mysql.h"

#define SOCKET int
#define INSERT 1
#define DELETE 2

using namespace std;
using namespace cv;

Mat getHandMask1(const Mat& image, int minCr, int maxCr, int minCb, int maxCb);
Point getHandCenter(const Mat& mask, double& radius);
int getFingerCount(const Mat& mask, Point center, double radius, double scale);
void *camera_finger_get(void *data);
void *DB_start(void *data);

void cleanup(void *arg)
{
	mysql_close((MYSQL *)arg);
	cout << "이전모드 종료"<<endl;
}
int MODE = 1;

int main( int argc, char **argv) {

	pthread_t p_thread[2];
	int thr_id;
	int status;


	
	thr_id = pthread_create(&p_thread[1], NULL, DB_start, NULL);
	if (thr_id < 0)
    	{
        perror("DB_thread create error : ");
        exit(0);
    	}

	thr_id = pthread_create(&p_thread[0], NULL, camera_finger_get, (void*)&p_thread[1]);
	if (thr_id < 0)
    	{
        perror("camera_thread create error : ");
        exit(0);
    	}
	
	pthread_detach(p_thread[1]);
	pthread_join(p_thread[0], (void **)&status);

	//camera_finger_get();
	//DB_start();

}

void *camera_finger_get(void *data){
	pthread_t DB_thread = *(pthread_t *)data;
    raspicam::RaspiCam_Cv Camera;
    Mat image,mask;
    Camera.set( CV_CAP_PROP_FORMAT, CV_8UC3);
    Camera.set( CV_CAP_PROP_FRAME_WIDTH, 640);
    Camera.set( CV_CAP_PROP_FRAME_HEIGHT, 480);

    if (!Camera.open()) {cerr<<"Error opening the camera"<<endl;return (void*)-1;}
	
	cout << "카메라 동작 시작 " <<endl;
	int now_finger = -1, pre_finger = -1;	
	int number = 0;

    while(1){
		
        Camera.grab();
        Camera.retrieve ( image);
		
		mask = getHandMask1(image, 133, 173, 77,127);
		erode(mask,mask,Mat(3,3,CV_8U, Scalar(1)), Point(-1,-1),2);
		
		double radius;
		Point center=getHandCenter(mask, radius);
		
		pre_finger = now_finger;
		now_finger = getFingerCount(mask, center,radius,2.0);

		if( now_finger != -1){
			//cout <<"손가락 개수 : "<< now_finger <<endl;
			if(now_finger == pre_finger){
				number++;
				cout << "모드 인식 중 (" << number << "/10)  " << now_finger << endl;  
			
				if(number == 10){
					if(now_finger == 0){
						MODE = INSERT;
						pthread_cancel(DB_thread);
						cout << "INSERT 모드 변경 완료 !"<<endl;
						int thr_id = pthread_create(&DB_thread, NULL, DB_start, NULL);
							if (thr_id < 0)
						    		{
						        	perror("DB_thread create error : ");
						        	exit(0);
    								}
						pthread_detach(DB_thread);
						number = 0;
					
					}
					if(now_finger == 5){
						MODE = DELETE;
						pthread_cancel(DB_thread);
						cout << "DELETE 모드 변경 완료 !"<<endl;
						int thr_id = pthread_create(&DB_thread, NULL, DB_start, NULL);
							if (thr_id < 0)
						    		{
						        	perror("DB_thread create error : ");
						        	exit(0);
    								}
						pthread_detach(DB_thread);
						number = 0;
					}
					else {number = 0;}
				
				}
			}
			else { number = 0;}			
		}

		

		circle(image, center, 2, Scalar(0,255, 0),-1);
		circle(image, center, (int)(radius)*2.0, Scalar(255, 0, 0), 2);
		circle(mask, center, 2, Scalar(0,255, 0),-1);
		circle(mask, center, (int)(radius)*2.0, Scalar(255, 0, 0), 2);


        imshow( "picamera test", image );
		imshow( "picamera test2", mask );
        if ( waitKey(20) == 27 ) break;

    }
	
    Camera.release();
   	return (void*) 0;

}



Mat getHandMask1(const Mat& image, int minCr, int maxCr, int minCb, int maxCb){
	
	
	
	Mat YCrCb;
	
	cvtColor(image, YCrCb, CV_BGR2YCrCb);
	
	/*inRange(YCrCb, Scalar(0, 133, 77), Scalar(255,173,127), YCrCb);

	Mat mask(image.size(), CV_8UC3, Scalar(0));

	add(image, Scalar(0), mask, YCrCb);	*/

	//각 채널별로 분리
	
	vector<Mat> planes;
	
	split(YCrCb, planes);
	
	 
	
	//각 채널별로 화소마다 비교
	
	Mat mask(image.size(), CV_8U, Scalar(0));   //결과 마스크를 저장할 영상
	
	int nr=image.rows;    //전체 행의 수
	
	int nc=image.cols;
	
	 
	
	for(int i=0; i<nr; i++){
	
		uchar* CrPlane=planes[1].ptr<uchar>(i);   //Cr채널의 i번째 행 주소
	
		uchar* CbPlane=planes[2].ptr<uchar>(i);   //Cb채널의 i번째 행 주소		
	
	for(int j=0; j<nc; j++){
	
		if( (minCr < CrPlane[j]) && (CrPlane[j] <maxCr) && (minCb < CbPlane[j]) && (CbPlane[j] < maxCb) )
	
		mask.at<uchar>(i, j)=255;
	
	}
	
	}

 

return mask;

}




Point getHandCenter(const Mat& mask, double& radius){

//거리 변환 행렬을 저장할 변수

	Mat dst;

	distanceTransform(mask, dst, CV_DIST_L2, 5);  //결과는 CV_32SC1 타입
	
	 
	
	//거리 변환 행렬에서 값(거리)이 가장 큰 픽셀의 좌표와, 값을 얻어온다.
	
	int maxIdx[2];    //좌표 값을 얻어올 배열(행, 열 순으로 저장됨)
	
	minMaxIdx(dst, NULL, &radius, NULL, maxIdx, mask);   //최소값은 사용 X
	
	 
	
	return Point(maxIdx[1], maxIdx[0]);

}

int getFingerCount(const Mat& mask, Point center, double radius, double scale){

	//손가락 개수를 세기 위한 원 그리기
	
	Mat cImg(mask.size(), CV_8U, Scalar(0));
	
	circle(cImg, center, radius*scale, Scalar(255));
	
	
	
	//원의 외곽선을 저장할 벡터
	
	vector<vector<Point>> contours;
	
	findContours(cImg, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);
	
	 
	
	if(contours.size()==0)   //외곽선이 없을 때 == 손 검출 X
	
	return -1;
	
	 
	
	//외곽선을 따라 돌며 mask의 값이 0에서 1로 바뀌는 지점 확인
	
	int fingerCount=0;
	
	for(int i=1; i<contours[0].size(); i++){
	
		Point p1=contours[0][i-1];
		
		Point p2=contours[0][i];
		
		if(mask.at<uchar>(p1.y, p1.x)==0 && mask.at<uchar>(p2.y, p2.x)>1)
		
		fingerCount++;
	
	}
	
	 
	
	//손목과 만나는 개수 1개 제외
	
	return fingerCount-1;

}

void *DB_start(void *data){

	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL);
	

	char barcode[14];
	char note_barcode[14] = "8801237665473";
	char query[1024] ;
	int len;
	const char *product_name = "notebook";
	int state;
	MYSQL* conn_ptr;
	MYSQL_RES* res;
	MYSQL_ROW row;


	conn_ptr = mysql_init(NULL);
	if(!conn_ptr){
		printf("DB 초기 화실패\n");
	}


	conn_ptr = mysql_real_connect(conn_ptr,"localhost","root","0929","fooo",3306,(char*)NULL,0);
	pthread_cleanup_push(cleanup, (void *)conn_ptr);

	if(conn_ptr){ printf("DB 연결성공\n");}
	else {printf(" 연결실패\n"); return (void*) -1;}
	

	if(MODE == INSERT){
		cout << "INSERT MODE 시작!!" << endl;
		while(1){cout << "this is insert"<<endl; sleep(1);}
		while(1){ 
		
			fgets(barcode,14,stdin);
			barcode[strlen(barcode)] = '\0';
			if(strcmp(barcode, note_barcode)==0){
	
				sprintf(query, "insert into product values('%s',%d,'%s')","8801237665473",3000,product_name);
				printf("입력 된커리 문: %s\n",query);
				
				state =  mysql_query(conn_ptr,query);
	
				if(state!=0){
					printf("DB Error : ");
					printf(mysql_error(conn_ptr));
				}
			}
	
			else {printf("fail\n"); }
	
		}
	}

	if(MODE == DELETE){
		cout << "DELETE MODE 시작!!" << endl;
		while(1){cout << "this is delete"<<endl; sleep(1);}
		while(1){ 
		
			fgets(barcode,14,stdin);
			barcode[strlen(barcode)] = '\0';
			if(strcmp(barcode, note_barcode)==0){
	
				sprintf(query, "insert into product values('%s',%d,'%s')","8801237665473",3000,product_name);
				printf("입력 된커리 문: %s\n",query);
				
				state =  mysql_query(conn_ptr,query);
	
				if(state!=0){
					printf("DB Error : ");
					printf(mysql_error(conn_ptr));
				}
			}
	
			else {printf("fail\n"); }
	
		}
	}
	pthread_cleanup_pop(0);
	mysql_close(conn_ptr);

}
