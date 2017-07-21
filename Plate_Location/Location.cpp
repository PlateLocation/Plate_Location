//////////////////////////////////////////////////////////////////////////
// Name:			Location.cpp
// Version:		1.0
// Date:			2017-3-23
// MDate:		2017-4-05
// Author:	    ˧����
// Desciption:  
// Defines CPlateLocate
// �޸�ʱ�䣺2017-4-14
// �޸����ݣ�������ɫ�ռ�Ķ�λ
//////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Location.h"
const float DEFAULT_ERROR = 0.6;
const float DEFAULT_ASPECT = 3.75;
const double WEIGHT_ONE = 0.33748;
const double WEIGHT_TWO = 0.66252;
const double AIM_WEIGHT = 0.7;
const double DEGREE_WEIGHT = 0.6;


CLocation::CLocation(Mat src, string name)
{
	/*m_srcImg.create(600, 800, TYPE);
	resize(src, m_srcImg, m_srcImg.size(), 0, 0, INTER_CUBIC);*/
	m_srcImg = src;
	m_deBug = 0;
	m_ImgName = name;
	m_dstImg = m_srcImg.clone();
	m_MorphSizeWidth = DEFAULT_MORPH_SIZE_WIDTH;
	m_MorphSizeHeight = DEFAULT_MORPH_SIZE_HEIGHT;
	m_error = DEFAULT_ERROR;
	m_aspect = DEFAULT_ASPECT;
	m_angle = DEFAULT_ANGLE;
	m_verifyMin = DEFAULT_VERIFY_MIN;
	m_verifyMax = DEFAULT_VERIFY_MAX;
	m_cols = src.cols;
	m_rows = src.rows;
	ColorFeture = new double *[m_rows];
	for (int i = 0; i < m_rows; i++)
		ColorFeture[i] = new double[m_cols]();
	m_featureNum = 0;
	m_featureSum = 0;
	m_featureMean = 0;
	m_aimWeight = 0;
	m_CharNum = 0;
	m_RuleDegree = 0;
	//��������ַ
	m_OutAddressFirst = "./src/out/";
	//��ַ+�ļ���
	strcpy(m_OutAddress, (m_OutAddressFirst + m_ImgName).c_str());
	m_aimWeight = AIM_WEIGHT;
}


CLocation::~CLocation()
{
}

void CLocation::PreTreatment(Mat src)
{
	Mat dst_Blur, dst_Gray;
	GaussianBlur(src, dst_Blur, Size(3, 3), 0, 0, BORDER_DEFAULT);			 //��˹ģ��
	cvtColor(dst_Blur, dst_Gray, CV_BGR2GRAY);								 //�ҶȻ�
	m_dstImg = dst_Gray;
}

///Sobel�������
void CLocation::SobelDetection()
{
	Mat src = m_dstImg;
	Mat grad_x, grad_y;
	Mat abs_grad_x, abs_grad_y;
	Mat grad;
	int scale = SOBEL_SCALE;
	int delta = SOBEL_DELTA;
	int ddepth = SOBEL_DDEPTH;
	/// X �ݶ�  
	//Scharr( src_gray, grad_x, ddepth, 1, 0, scale, delta, BORDER_DEFAULT );  
	//Calculates the first, second, third, or mixed image derivatives using an extended Sobel operator.  
	Sobel(src, grad_x, ddepth, 1, 0, 3, scale, delta, BORDER_DEFAULT);
	convertScaleAbs(grad_x, abs_grad_x);
	/// Y �ݶ�    
	//Scharr( src_gray, grad_y, ddepth, 0, 1, scale, delta, BORDER_DEFAULT );  
	Sobel(src, grad_y, ddepth, 0, 1, 3, scale, delta, BORDER_DEFAULT);
	convertScaleAbs(grad_y, abs_grad_y);
	/// Total Gradient (approximate)  
	addWeighted(abs_grad_x, 0.5, abs_grad_y, 0.5, 0, grad);
	m_dstImg = grad;

	threshold(m_dstImg, m_dstImg, 0, 255, CV_THRESH_OTSU + CV_THRESH_BINARY); //��ֵ��

	if (m_deBug)
	{
		imshow("Sobel��ֵ��", m_dstImg);
		cvWaitKey(0);
	}
	Mat element = getStructuringElement(MORPH_RECT, Size(25, 2)); //���ú���25*2����ģ��

	morphologyEx(m_dstImg, m_dstImg, MORPH_CLOSE, element);			//��̬ѧ����

	if (m_deBug)
	{
		imshow("Sobel����̬ѧ", m_dstImg);
		cvWaitKey(0);
	}
}

///Canny�������
void CLocation::CannyDetection()
{
	IplImage imgTmp = m_dstImg;
	IplImage* pImg = cvCloneImage(&imgTmp);
	IplImage* pCannyImg = NULL;

	double low_thresh = 0.0;
	double high_thresh = 0.0;

	pCannyImg = cvCreateImage(cvGetSize(pImg), IPL_DEPTH_8U, 1);

	CvMat *dx = (CvMat*)pImg;
	CvMat *dy = (CvMat*)pCannyImg;
	if (low_thresh == 0.0 && high_thresh == 0.0)
	{
		AdaptiveFindThreshold(pImg, &low_thresh, &high_thresh, 3);
		cout << "low_thresh:  " << low_thresh << endl;
		cout << "high_thresh: " << high_thresh << endl;
	}
	cvCanny(pImg, pCannyImg, low_thresh, high_thresh, 3);
	m_dstImg = cvarrToMat(pCannyImg, true);

	IplImage qImg = IplImage(m_dstImg); // cv::Mat -> IplImage
	cvSaveImage("./src/out/Canny.jpg", &qImg);

	imshow("canny", m_dstImg);
	cvWaitKey(0);
}


//����ȡCanny������ֵ
void CLocation::AdaptiveFindThreshold(const CvArr* image, double *low, double *high, int aperture_size)
{
	Mat src = cvarrToMat(image, true);
	const int cn = src.channels();
	Mat dx(src.rows, src.cols, CV_16SC(cn));
	Mat dy(src.rows, src.cols, CV_16SC(cn));

	Sobel(src, dx, CV_16S, 1, 0, aperture_size, 1, 0, cv::BORDER_REPLICATE);
	Sobel(src, dy, CV_16S, 0, 1, aperture_size, 1, 0, cv::BORDER_REPLICATE);

	CvMat _dx = dx, _dy = dy;
	_AdaptiveFindThreshold(&_dx, &_dy, low, high);

}

// ����matlab������Ӧ��ߵ���������                                              
void CLocation::_AdaptiveFindThreshold(CvMat *dx, CvMat *dy, double *low, double *high)
{
	CvSize size;
	IplImage *imge = 0;
	int i, j;
	CvHistogram *hist;
	int hist_size = 255;
	float range_0[] = { 0,256 };
	float* ranges[] = { range_0 };
	double PercentOfPixelsNotEdges = 0.7;
	size = cvGetSize(dx);
	imge = cvCreateImage(size, IPL_DEPTH_32F, 1);
	// �����Ե��ǿ��, ������ͼ����                                          
	float maxv = 0;
	for (i = 0; i < size.height; i++)
	{
		const short* _dx = (short*)(dx->data.ptr + dx->step*i);
		const short* _dy = (short*)(dy->data.ptr + dy->step*i);
		float* _image = (float *)(imge->imageData + imge->widthStep*i);
		for (j = 0; j < size.width; j++)
		{
			_image[j] = (float)(abs(_dx[j]) + abs(_dy[j]));
			maxv = maxv < _image[j] ? _image[j] : maxv;

		}
	}
	if (maxv == 0) {
		*high = 0;
		*low = 0;
		cvReleaseImage(&imge);
		return;
	}

	// ����ֱ��ͼ                                                            
	range_0[1] = maxv;
	hist_size = (int)(hist_size > maxv ? maxv : hist_size);
	hist = cvCreateHist(1, &hist_size, CV_HIST_ARRAY, ranges, 1);
	cvCalcHist(&imge, hist, 0, NULL);
	int total = (int)(size.height * size.width * PercentOfPixelsNotEdges);
	float sum = 0;
	int icount = hist->mat.dim[0].size;

	float *h = (float*)cvPtr1D(hist->bins, 0);
	for (i = 0; i < icount; i++)
	{
		sum += h[i];
		if (sum > total)
			break;
	}
	// ����ߵ�����                                                          
	*high = (i + 1) * maxv / hist_size;
	*low = *high * 0.4;
	cvReleaseImage(&imge);
	cvReleaseHist(&hist);

}


//! ��ʾ�������ɵĳ���ͼ�񣬱����ж��Ƿ�ɹ���������ת��
Mat CLocation::showResultMat(Mat src, Size rect_size, Point2f center, int index)
{
	Mat img_crop;

	getRectSubPix(src, rect_size, center, img_crop);

	Mat resultResized;

	resultResized.create(HEIGHT, WIDTH, TYPE);

	resize(img_crop, resultResized, resultResized.size(), 0, 0, INTER_CUBIC);

	return resultResized;
}

// Defines CPlateLocate
// �޸�ʱ�䣺2017-4-14
// �޸����ݣ���ɫ������ȡ

//�� ��ɫ������ȡ
bool CLocation::ColorFeatureExtraction()
{
	Mat src = m_srcImg.clone(); //���
	int cPointB, cPointG, cPointR;
	for (int i = 1; i < m_rows; i++)
	{
		for (int j = 1; j < m_cols; j++)
		{
			cPointB = src.at<Vec3b>(i, j)[0];
			cPointG = src.at<Vec3b>(i, j)[1];
			cPointR = src.at<Vec3b>(i, j)[2];

			int dif_B_R = cPointB - cPointR; //������ɫ����
			int dif_B_G = cPointB - cPointG;
			if (dif_B_G > 0 && dif_B_R > 0)
			{
				ColorFeture[i][j] = WEIGHT_ONE * dif_B_R + WEIGHT_TWO * dif_B_G;
				if (ColorFeture[i][j] > 0)
				{
					m_featureNum++;   //ͳ�Ʒ�0������������
					m_featureSum += ColorFeture[i][j] / 1000; //������������ֵ�ܺ�
				}
			}
			else
				ColorFeture[i][j] = 0;
		}
	}
	if (m_featureNum > 0)
	{
		m_featureMean = m_featureSum / m_featureNum; //����������ֵ
		m_featureMean *= 1000;
	}
	int m_aimNum = 0;
	for (int i = 1; i < m_rows; i++)
	{
		for (int j = 1; j < m_cols; j++)
		{
			if (ColorFeture[i][j] > m_featureMean && ColorFeture[i][j] > 0) //��ȡĿ�������
				m_aimNum++;
		}
	}

	m_aimWeight = m_aimNum *1.0 / m_featureNum; //��ȡĿ���Ȩֵ
	//cout << m_aimWeight << " " << m_aimNum << " " << m_featureNum << endl;
	Mat dst = src;
	dst = Binarization(dst, m_featureMean, m_aimWeight); //����Ŀ�������ֵ��

	//imshow("��ֵ��", dst);
	//cvWaitKey(0);
	if (m_deBug)
	{
		imshow("��ɫ+Sobel��ֵ��", dst);
		cvWaitKey(0);
	}

	dst = Morphological(dst); //��̬ѧ����

	if (m_deBug)
	{
		imshow("��ɫ+Sobel��ֵ������̬ѧ", dst);
		cvWaitKey(0);
	}
	//ContourTracking(dst);

	cvtColor(dst, dst, CV_BGR2GRAY);

	if (ContourSearch(dst)) //ɸѡ����
		return true;
	else
		return false;
}

//������Ŀ�������ֵ��
Mat CLocation::Binarization(Mat src, double mean, double aimWeight)
{
	Mat dst = src;
	double thresh = mean*(m_aimWeight + (1 - m_aimWeight) / aimWeight); // Ŀ���
	//cout << thresh << endl;
	//��ֵ��
	for (int i = 1; i < m_rows; i++)
	{
		for (int j = 1; j < m_cols; j++)
		{
			//�����ڸ���ֵ���ڱ�Ե����Ľ������ֵ255
			if (ColorFeture[i][j] > thresh && (int)m_dstImg.at<uchar>(i, j) == 255)
			{
				dst.at<Vec3b>(i, j)[0] = 255;
				dst.at<Vec3b>(i, j)[1] = 255;
				dst.at<Vec3b>(i, j)[2] = 255;
				ColorFeture[i][j] = 1;
			}
			else
			{
				dst.at<Vec3b>(i, j)[0] = 0;
				dst.at<Vec3b>(i, j)[1] = 0;
				dst.at<Vec3b>(i, j)[2] = 0;
				ColorFeture[i][j] = 0;
			}
		}
	}
	return dst;
}

//����̬ѧ����
Mat CLocation::Morphological(Mat src)
{
	//imshow("��̬ѧǰ", src);
	//cvWaitKey(0);

	Mat element = getStructuringElement(MORPH_RECT, Size(25, 2)); //�Ⱥ���25*2
	//��̬ѧ����
	morphologyEx(src, src, MORPH_CLOSE, element);

	//imshow("��һ����̬ѧ", src);
	//cvWaitKey(0);

	element = getStructuringElement(MORPH_RECT, Size(5, 2)); //����5*2
	//��̬ѧ����
	morphologyEx(src, src, MORPH_OPEN, element);

	//imshow("�ڶ�����̬ѧ", src);
	//cvWaitKey(0);

	element = getStructuringElement(MORPH_RECT, Size(2, 5)); //����2*5
	//��̬ѧ����
	morphologyEx(src, src, MORPH_OPEN, element);

	//imshow("��������̬ѧ", src);
	//cvWaitKey(0);

	return src;
}

bool CLocation::ContourSearch(Mat src)
{
	//Find ���� of possibles plates
	vector< vector< Point> > contours;
	findContours(src,
		contours, // a vector of contours
		CV_RETR_EXTERNAL, // ��ȡ�ⲿ����
		CV_CHAIN_APPROX_NONE); // ÿ����������������

	vector<vector<Point> >::iterator itc = contours.begin();

	vector<RotatedRect> rects;

	int t = 0;
	while (itc != contours.end())
	{
		//����minAreaRect()������С��Ӿ���
		RotatedRect mr = minAreaRect(Mat(*itc));
	
		//�ж��Ƿ�������߱� ��������ɾ��
		if (!verifySizes(mr))
		{
			itc = contours.erase(itc);
		}
		else
		{
			++itc;
			rects.push_back(mr);
		}
	}
	int k = 1;
	vector<Mat> resultVec;
	for (int i = 0; i < rects.size(); i++)
	{
		RotatedRect minRect = rects[i];
		if (verifySizes(minRect))
		{
			//��бУ�� ���Խ�һ��ɸ������ͼƬ
			float r = (float)minRect.size.width / (float)minRect.size.height;
			float angle = minRect.angle;
			Size rect_size = minRect.size;
			if (r < 1)
			{
				angle = 90 + angle;
				swap(rect_size.width, rect_size.height);
			}
			//���ץȡ�ķ�����ת����m_angle�Ƕȣ����ǳ��ƣ���������
			if (angle - m_angle < 0 && angle + m_angle > 0)
			{
				//Create and rotate image
				Mat rotmat = getRotationMatrix2D(minRect.center, angle, 1);
				Mat img_rotated;
				//����
				warpAffine(m_srcImg, img_rotated, rotmat, m_dstImg.size(), CV_INTER_CUBIC);

				//Mat resultMat(img_rotated, minRect);
				Mat resultMat;
				resultMat = showResultMat(img_rotated, rect_size, minRect.center, k++);
				resultVec.push_back(resultMat);
			}
		}
	}

	double DegreeMax = -1;
	int LastKey = -1;
	for (int k = 0; k < resultVec.size(); k++)
	{

		Mat TempDst;
		cvtColor(resultVec[k], TempDst, CV_BGR2GRAY);

		int graySum = 0;
		for (int i = 1; i < TempDst.rows; i++)
		{
			for (int j = 1; j < TempDst.cols; j++)
			{
				graySum += (int)TempDst.at<uchar>(i, j);
			}
		}

		int grayMean = graySum / (TempDst.rows* TempDst.cols);//��Ҷ�ֵ��ֵ


		//�Աȶ���ǿ ���Ҷ�ֵС�ھ�ֵ��Ԫ�ظ�0
		for (int i = 1; i < TempDst.rows; i++)
		{
			for (int j = 1; j < TempDst.cols; j++)
			{
				if ((int)TempDst.at<uchar>(i, j) < grayMean)
					TempDst.at<uchar>(i, j) = 0;
				else
				{
					TempDst.at<uchar>(i, j) = (int)TempDst.at<uchar>(i, j)*((int)TempDst.at<uchar>(i, j) - grayMean) / (255 - grayMean);

				}
			}
		}

		threshold(TempDst, TempDst, 0, 255, CV_THRESH_OTSU + CV_THRESH_BINARY); //��ֵ��
	//	imshow("da", TempDst);
	//	waitKey(0);
		// ���г�������ֵ����
		double Degree = VerticalProjection(TempDst);

		//���ƶ�Ӧ��������ֵ
		if (Degree > DegreeMax)
		{
			DegreeMax = Degree;
			LastKey = k;
		}
	}
	if (m_deBug)
	{
		if (resultVec.size() != 0)
		{
			imshow("result", resultVec[LastKey]);
			cvWaitKey(0);
		}
	}
	//imshow("Last", resultVec[LastKey]);
	if (DegreeMax != -1 || LastKey != -1)
	{
		IplImage qImg = IplImage(resultVec[LastKey]); // cv::Mat -> IplImage
		cvSaveImage(m_OutAddress, &qImg);
		printf("�ɹ�\n");
		vector< vector< Point> >().swap(contours);
		vector< RotatedRect >().swap(rects);
		vector< Mat >().swap(resultVec);
		return true;
	}
	vector< vector< Point> >().swap(contours);
	vector< RotatedRect >().swap(rects);
	vector< Mat >().swap(resultVec);
	printf("ʧ��\n");
	return false;


}

//! ��minAreaRect��õ���С��Ӿ��Σ����ݺ�Ƚ����ж�
bool CLocation::verifySizes(RotatedRect mr)
{
	float error = m_error;
	//Spain car plate size: 52x11 aspect 4,7272
	//China car plate size: 440mm*140mm��aspect 3.142857
	float aspect = m_aspect;
	//Set a min and max area. All other patchs are discarded
	//int min= 1*aspect*1; // minimum area
	//int max= 2000*aspect*2000; // maximum area
	int min = 44 * 14 * m_verifyMin; // minimum area
	int max = 44 * 14 * m_verifyMax; // maximum area
									 //Get only patchs that match to a respect ratio.
	float rmin = aspect - aspect*error;
	float rmax = aspect + aspect*error;

	int area = mr.size.height * mr.size.width;
	float r = (float)mr.size.width / (float)mr.size.height;
	if (r < 1)
	{
		r = (float)mr.size.height / (float)mr.size.width;
	}

	if ((area < min || area > max) || (r < rmin || r > rmax))
	{
		return false;
	}
	else
	{
		return true;
	}
}

double CLocation::VerticalProjection(Mat src)
{
	m_Projection = new int[src.cols]();
	//memset(m_Projection, 0, sizeof(m_Projection));
	for (int j = 1; j < src.cols; j++)
	{
		//��������20% ֻȡ�м�60%��ͳ��
		for (int i = 1 + 0.2*src.rows; i < src.rows*(1 - 0.2); i++)
		{
			if ((int)src.at<uchar>(i, j) > 0)
			{
				m_Projection[j]++;
			}
		}
	}
	bool CharState = false;
	int *m_CharWidth = new int[src.cols]();
	m_CharNum = 0;
	//ͳ���ַ�����
	for (int i = 1; i < src.cols; i++)
	{
		if (m_Projection[i] != 0 && !CharState)
		{
			CharState = true;
		}
		else if (!m_Projection[i] && CharState)
		{
			m_CharNum++;
			CharState = false;
		}
		if (CharState)
			m_CharWidth[m_CharNum]++;
	}
	double wid = src.cols / 10;
	int LastNum = m_CharNum;
	double TempNum = 0;
	for (int i = 0; i < m_CharNum; i++)
	{
		if (m_CharWidth[i] < wid / 4)
			LastNum--;
		else if (m_CharWidth[i] < wid / 3 || m_CharWidth[i] > wid * 1.5)
			TempNum++;
	}
	if (LastNum < 7)
		m_RuleDegree = 0;
	else
	{
		//�����ѡ��������
		//	1 - 0.6*(abs(7 - LastNum)) / max(7, LastNum) - 0.4*(TempNum / LastNum);
		m_RuleDegree = 1 - DEGREE_WEIGHT*(abs(DEFAULT_CHAR_NUM - LastNum) *1.0 / max(DEFAULT_CHAR_NUM, LastNum)) - (1 - DEGREE_WEIGHT)*(TempNum *1.0 / LastNum);
	}
	return m_RuleDegree;
}

//////////////////////////////////////////////////////////////////////////
// Author:	    ˧����
// �޸�ʱ�䣺2017-7-10
// �޸����ݣ���������
//////////////////////////////////////////////////////////////////////////

void CLocation::ContourTracking(Mat src)
{
	int num;
	int fill_value;
	int xs, ys;
	Mat dst = src.clone();
	for (int i = 0; i < src.rows; i++)
	{
		for (int j = 0; j < src.cols; j++)
		{
			dst.at<Vec3b>(i, j)[0] = 0;
			dst.at<Vec3b>(i, j)[1] = 0;
			dst.at<Vec3b>(i, j)[2] = 0;
		}
	}
	for (int y = 0; y < src.rows; y++)
	{
		for (int x = 0; x < src.cols; x++)
		{
			if ((int)src.at<uchar>(y, x) == MAX_BRIGHTNESS)
			{
				num = ContourMarking(y, x, src);
				if (num > MAX_PERIMETER)
					fill_value = MAX_BRIGHTNESS;
				else fill_value = GRAY;
				xs = x;  ys = y;
				//src.at<uchar>(ys, xs) = 0;
				src.at<Vec3b>(ys, xs)[0] = 0;
				src.at<Vec3b>(ys, xs)[1] = 0;
				src.at<Vec3b>(ys, xs)[2] = 0;
				dst.at<Vec3b>(ys, xs)[0] = fill_value;
				dst.at<Vec3b>(ys, xs)[1] = fill_value;
				dst.at<Vec3b>(ys, xs)[2] = fill_value;
				if (num > 1) {
					for (int i = 0; i < num - 1; i++) {
						xs = xs + Freeman[chain_code[i]][0];
						ys = ys + Freeman[chain_code[i]][1];
						src.at<Vec3b>(ys, xs)[0] = 0;
						src.at<Vec3b>(ys, xs)[1] = 0;
						src.at<Vec3b>(ys, xs)[2] = 0;
						dst.at<Vec3b>(ys, xs)[0] = fill_value;
						dst.at<Vec3b>(ys, xs)[1] = fill_value;
						dst.at<Vec3b>(ys, xs)[2] = fill_value;
					}
				}
			}

		}
	}
	for (int y = 0; y < src.rows; y++) {
		for (int x = 0; x < src.cols; x++) {
			if ((int)dst.at<uchar>(y, x) == GRAY) {
				dst.at<Vec3b>(y, x)[0] = 0;
				dst.at<Vec3b>(y, x)[1] = 0;
				dst.at<Vec3b>(y, x)[2] = 0;
				for (int i = 0; i < 8; i++) {
					xs = x + Freeman[i][0];
					ys = y + Freeman[i][1];
					if (xs >= 0 && xs <= src.cols &&
						ys >= 0 && ys <= src.rows &&
						(int)dst.at<uchar>(ys, xs) == MAX_BRIGHTNESS)
						dst.at<Vec3b>(y, x)[0] = MAX_BRIGHTNESS;
						dst.at<Vec3b>(y, x)[1] = MAX_BRIGHTNESS;
						dst.at<Vec3b>(y, x)[2] = MAX_BRIGHTNESS;
				}
			}
		}
	}
	imshow("��������", dst);
	cvWaitKey(0);
}

int CLocation::ContourMarking(int x_start, int y_start, Mat src)
{
	int  x, y;             /* ݆�����ϤάF�ڤ�עĿ���ؤ����� */  /*��ǰ��Ŀ�����ص�������������*/
	int xs, ys;             /* עĿ���ؤ��܇��̽���������   */  /*������ĸ���Ȥ��������Χ������*/
	int code, num;          /* ݆����Υ����`�󥳩`��, �t��   */  /*����������룬����*/
	int i, counter, detect; /* ���������ʤ�                   */  /*��Լ����*/
	counter = 0;			/* ������Υ����å� */  /*��������*/
	for (i = 0; i < 8; i++) {
		xs = x_start + Freeman[i][0];
		ys = y_start + Freeman[i][1];
		if (xs >= 0 && xs <= src.cols && ys >= 0 && ys <= src.rows
			&& (int)src.at<uchar>(ys, xs) == MAX_BRIGHTNESS) counter++;
	}
	if (counter == 0) num = 1;  /* (x_start,y_start)�Ϲ����� */ /*����ǹ�����*/
	else {
		/* ̽���_ʼ */
		num = -1;   x = x_start;    y = y_start;    code = 0;
		do {
			detect = 0; /* �Τε��ߤĤ����Ȥ����ˤ��� */
						/* ����̽������ΛQ�� */   /*��ʼ�����ȷ��*/
			code = code - 3;   if (code < 0) code = code + 8;
			do {
				xs = x + Freeman[code][0];
				ys = y + Freeman[code][1];
				if (xs >= 0 && xs <= src.cols && ys >= 0 &&
					ys <= src.rows &&
					(int)src.at<uchar>(ys, xs) == MAX_BRIGHTNESS) {
					detect = 1;  /* �Τε��ʳ� */  /*��һ����ļ��*/
					num++;
					if (num > MAX_CNTR) {
						printf("݆�����λ����� > %d\n", MAX_CNTR);
						exit(1);
					}
					chain_code[num] = code;
					x = xs;  y = ys;
				}
				code++;  if (code > 7) code = 0;
			} while (detect == 0);
		} while (x != x_start || y != y_start); /* �_ʼ��Ηʳ��ޤ� */  /*������*/
		num = num + 2;  /* chain_code[ ]�������֤ȤΤ�������� */  /*chain_code[]���±�ƫ����޸�*/
	}
	return(num);
}


bool CLocation::Color_Contour()
{
	
	Mat dst = m_srcImg.clone();
	GaussianBlur(m_srcImg, srcBlur, Size(3, 3), 0, 0, BORDER_DEFAULT);
	//cvtColor(src, srcHSV, CV_BGR2HSV);
	int H, S, V;
	for (int i = 1; i < m_rows - 2; i++)
	{
		for (int j = 1; j < m_cols - 2; j++)
		{
			//H = (int)srcHSV.at<Vec3b>(i, j)[0];
			//S = (int)srcHSV.at<Vec3b>(i, j)[1];
			//V = (int)srcHSV.at<Vec3b>(i, j)[2];
			bool blue_status = false;
			bool white_status = false;
			for (int x = -1; x <= 1; x += 2)
			{
				int blue_count = 0;
				int white_count = 0;

				//for (int y = -1; y <= 0; y++)
				{
					if (Blue_Judge(i, j + x) && !blue_status)
						blue_count++;
					if (White_Judge(i, j + x) && !white_status)
						white_count++;
				}
				if (blue_count >= 1)
					blue_status = true;
				if (white_count >= 1)
					white_status = true;
			}
			if (blue_status&&white_status)
			{
				//for (int x = -1; x <= 1; x++)
				{
					for (int y = -1; y <= 1; y++)
					{
						Color_Mark[i + y][j] = 1;
						dst.at<Vec3b>(i + y, j)[0] = 255;
						dst.at<Vec3b>(i + y, j)[1] = 255;
						dst.at<Vec3b>(i + y, j)[2] = 255;
					}
				}
				//	cout << i << " " << j << endl;
			}
			else
			{
				for (int y = -1; y <= 1; y++)
				{
					Color_Mark[i + y][j] = 0;
					dst.at<Vec3b>(i + y, j)[0] = 0;
					dst.at<Vec3b>(i + y, j)[1] = 0;
					dst.at<Vec3b>(i + y, j)[2] = 0;
				}
			}
		}
	}
	imshow("test0", dst);
	cvWaitKey(0);
	cvtColor(dst, dst, CV_RGB2GRAY);
	threshold(dst, dst, 0, 255, CV_THRESH_OTSU + CV_THRESH_BINARY);
	Mat element = getStructuringElement(MORPH_RECT, Size(25, 5)); //���ú���25*2����ģ��
	morphologyEx(dst, dst, MORPH_CLOSE, element);			//��̬ѧ����
	imshow("��̬ѧ", dst);
	cvWaitKey(0);
	/*for (int i = 0; i < dst.rows; i++)
	{
		for (int j = 0; j < dst.cols; j++)
		{
			if ((int)dst.at<uchar>(i, j) == MAX_BRIGHTNESS)
				image1[i][j] = 255;
			else
				image1[i][j] = 0;
		}
	}*/

	if (ContourSearch(dst)) //ɸѡ����
		return true;
	else
		return false;

}

bool CLocation::Blue_Judge(int x, int y)
{
	int b = srcBlur.at<Vec3b>(x, y)[0];
	int g = srcBlur.at<Vec3b>(x, y)[1];
	int r = srcBlur.at<Vec3b>(x, y)[2];
	if (b *1.0> 1.4*g*1.0&&b*1.0 > 1.4*r*1.0&&b > 100)
		return true;
	return false;
}

bool CLocation::White_Judge(int x, int y)
{
	int b = (int)srcBlur.at<Vec3b>(x, y)[0];
	int g = (int)srcBlur.at<Vec3b>(x, y)[1];
	int r = (int)srcBlur.at<Vec3b>(x, y)[2];
	double S = b + g + r;
	if (b*1.0 < 0.4*S&&g*1.0 < 0.4*S&&r*1.0 < 0.4*S&&S>200)
		return true;
	return false;
}