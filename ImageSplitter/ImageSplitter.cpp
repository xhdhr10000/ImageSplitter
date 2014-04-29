#include <atlimage.h>
#include <math.h>
#include <memory.h>
#include <time.h>

#define MAX_PIC		4096
#define MAX_Q		1600000
#define TOLERANCE	5
#define DEFAULT_PIC	"default.jpg"

class Point
{
public:
	Point():x(0),y(0) {}
	Point(int px, int py):x(px),y(py) {}
	Point operator+(const Point &p) {
		return Point(x+p.x,y+p.y);
	}
	Point operator*(const int p) {
		return Point(x*p,y*p);
	}
	int x, y;
};

class Rect
{
public:
	int left, top, right, bottom;
};

Point dir[4] = { Point(0,-1), Point(1,0), Point(0,1), Point(-1,0) };
CImage img, nimg;
char *addr;
int w, h, bpp, pitch, line_length;
bool hash[MAX_PIC][MAX_PIC], hash2[MAX_PIC][MAX_PIC];
Point queue[MAX_Q];
int head, rear;
int outcount=0;

COLORREF pixel_diff(COLORREF a, COLORREF b=0xffffff)
{
	return abs((long)(a&0xff)-(long)(b&0xff)) +
		abs((long)(a&0xff00>>8)-(long)(b&0xff00>>8)) +
		abs((long)(a&0xff0000>>16)-(long)(b&0xff0000>>16));
}

COLORREF getPixel(int x, int y)
{
	if (pitch<0) return addr[(h-y-1)*line_length+x*bpp/8]&0xffffff;
	else return addr[y*line_length+x*bpp/8]&0xffffff;
}

int main(int argc, char* argv[])
{
	clock_t t1, t2;
	char pic_input[MAX_PATH];

	gets(pic_input);
	if (img.Load(pic_input))
		img.Load(DEFAULT_PIC);
	img.Save("tmp.png", Gdiplus::ImageFormatPNG);
	img.Destroy();
	img.Load("tmp.png");

	t1 = clock();
	w = img.GetWidth();
	h = img.GetHeight();
	bpp = img.GetBPP();
	line_length = w*bpp/8+w*bpp/8%4;
	pitch = img.GetPitch();
	if (pitch > 0)
		addr = (char*)img.GetPixelAddress(0,0);
	else
		addr = (char*)img.GetPixelAddress(0,h-1);
	printf("Picture format: %dx%d-%d pitch:%d\n", w, h, bpp, pitch);

	memset(hash, false, sizeof(hash));
	for (int j=0; j<h; j++) {
		for (int i=0; i<w; i++) {
			if (hash[i][j]) continue;
			if (pixel_diff(getPixel(i,j))>=0x7f) {
				/* Flood-fill */
				Point min = Point(i,j), max = Point(i,j);
				hash[i][j] = true;
				head=-1; rear=0;
				queue[0] = Point(i,j);
				while (head != rear) {
					head = (head+1)%MAX_Q;
					for (int k=0; k<4; k++) {
						for (int l=1; l<=TOLERANCE; l++) {
							Point tmp = queue[head]+dir[k]*l;
							if (tmp.x<0 || tmp.x>=w || tmp.y<0 || tmp.y>=h) continue;
							if (hash[tmp.x][tmp.y]) continue;
							if (pixel_diff(getPixel(tmp.x,tmp.y))<0x7f) continue;
							rear = (rear+1)%MAX_Q;
							queue[rear] = tmp;
							hash[tmp.x][tmp.y] = true;
							if (tmp.x<min.x) min.x = tmp.x;
							if (tmp.x>max.x) max.x = tmp.x;
							if (tmp.y<min.y) min.y = tmp.y;
							if (tmp.y>max.y) max.y = tmp.y;
						}
					}
				}
				if (max.x-min.x<=5 || max.y-min.y<=5) continue;
				printf("Got region: (%d,%d)-(%d,%d)\n", min.x, min.y, max.x, max.y);

				/* Expand to the frames */
				Rect border = {min.x-1, min.y-1, max.x+1, max.y+1};
				memset(hash2, false, sizeof(hash2));
				for (int i=min.x; i<=max.x; i++)
					for (int j=min.y; j<=max.y; j++) hash2[i][j] = true;
				head=-1; rear=0;
				queue[rear++] = Point(min.x-1, min.y-1);
				queue[rear++] = Point(max.x+1, min.y-1);
				queue[rear++] = Point(max.x+1, max.y+1);
				queue[rear++] = Point(min.x-1, max.y+1);
				while (head != rear) {
					head = (head+1)%MAX_Q;
					for (int k=0; k<4; k++) {
						Point tmp = queue[head]+dir[k];
						if (tmp.x<0 || tmp.x>=w || tmp.y<0 || tmp.y>=h) continue;
						if (hash2[tmp.x][tmp.y]) continue;
						if (pixel_diff(getPixel(tmp.x,tmp.y))>=0x7f) continue;
						rear = (rear+1)%MAX_Q;
						queue[rear] = tmp;
						hash2[tmp.x][tmp.y] = true;
						if (tmp.x<border.left) border.left = tmp.x;
						if (tmp.x>border.right) border.right = tmp.x;
						if (tmp.y<border.top) border.top = tmp.y;
						if (tmp.y>border.bottom) border.bottom = tmp.y;
					}
					if (border.left<=0 || border.top<=0 || border.right>=w-1 || border.bottom>=h-1) break;
				}
				printf("Expand to: (%d,%d)-(%d,%d)\n", border.left,border.top,border.right,border.bottom);

				if (border.left>0 && border.top>0 && border.right<w-1 && border.bottom<h-1 &&
					border.right-border.left>=5 && border.bottom-border.top>=5) {
						/* Output */
						int x,y,xx,yy;
						nimg.Create(border.right-border.left-1,border.bottom-border.top-1,bpp);
						for (x=border.left+1,xx=0; x<border.right; x++,xx++)
							for (y=border.top+1,yy=0; y<border.bottom; y++,yy++)
								nimg.SetPixel(xx,yy,getPixel(x,y));
						TCHAR outname[MAX_PATH];
						_stprintf(outname, "out%d.jpg", ++outcount);
						nimg.Save(outname);
						nimg.Destroy();

						for (int i=border.left; i<=border.right; i++)
							for (int j=border.top; j<=border.bottom; j++) hash[i][j] = true;
				}
			}
		}
	}
	img.Destroy();
	t2 = clock();
	printf("Process time: %f\n", (float)(t2-t1)/1000.0f);
	return 0;
}