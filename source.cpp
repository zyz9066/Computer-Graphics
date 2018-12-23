#pragma region header
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <string.h>
#include <GL/glut.h>
#include <GL/glui.h>
#include <vector>

using namespace std;
#pragma endregion

#pragma region define
#define FILE_BROWSER 100

#define VIEW_XY 0
#define VIEW_YZ 1
#define VIEW_XZ 2
#define VIEW_PERSP 3

#define OBJ_SMOOTH 3
#define OBJ_FLAT 2
#define OBJ_WIREFRAME 1
#define OBJ_POINTS 0

#define TRANSFORM_NONE    20
#define TRANSFORM_ROTATE  21
#define TRANSFORM_SCALE 22
#define TRANSFORM_TRANSLATE 23
#pragma endregion

#pragma region variables
//live variables
int winH = 650, winW = 750;
int obj_type = 1;
int main_window;
int show_ground=1;
int show_bb=0;
int show_axes = 1;
int show_mesh = 0;

int proj_mode = VIEW_PERSP, obj_mode=OBJ_SMOOTH, xform_mode = TRANSFORM_NONE;
int rotate_on = 1;

int press_x, press_y;
float x_angle = 0.f, y_angle = 0.f;
float tx = 0.f, ty = 0.f; //translation vector
float scale_size = 1.f, obj_scale, x_scale, y_scale, z_scale;
float px, py, pz; //centre of object
#pragma endregion

#pragma region GL varibles
// Pointers to glui
GLUI *glui;
GLUI_RadioGroup *radio;
GLUI_Panel      *obj_panel;
GLUI_FileBrowser *fb;

// Miscellaneous global variables
GLfloat light_p[] = { 1.f, 1.f, 5.f, 0.f }; // light position
GLfloat light_s[] = { 1.0f, 1.0f, 1.0f, 1.0f };
GLfloat light_d[] = { 1.0f, 1.0f, 1.0f, 1.0f };// light color
GLfloat lmodel_ambient[] = { 1.f, 0.2f, 0.2f, 1.0f };

GLfloat mat_a[] = { .1f, .1f, .1f, 1.f };
GLfloat mat_d[] = { .8f,.8f,.8f,1.f };
GLfloat mat_s[] = { 1.f,1.f,1.f,1.0f};
GLfloat mat_shininess[] = { 50.f }; // Phong exponent
GLfloat mat_e[] = { .0f,.0f,.0f,1.0f};
#pragma endregion

#pragma region half edge data structure
struct HE_vert;
struct HE_face;

struct HE_edge {
	HE_vert* vert; // vertex at the end of the half-edge
	HE_edge* pair; // oppositely oriented half-edge
	HE_face* face; // the incident face
	HE_edge* prev; // previous half-edge around the face
	HE_edge* next; // next half-edge around the face
	int flag = 1;
};
struct HE_vert {
	float x, y, z; // the vertex coordinates
	float nx, ny, nz;//vertex normal
	HE_edge* edge; // one of the half-edges emanating from the vertex
	vector<HE_edge*> temp;
};
struct HE_face {
	HE_edge* edge; // one of the half-edges bordering the face
	float nx, ny, nz;
	float area;
};

vector<HE_edge*> he_edge;
vector<HE_vert*> he_vert;
vector<HE_face*> he_face;
#pragma endregion

#pragma region compute normal
void f_norm(HE_face* f) {

	float x0, y0, z0, x1, y1, z1, x2, y2, z2;
	float a, b, c, d;

	x0 = f->edge->vert->x;
	y0 = f->edge->vert->y;
	z0 = f->edge->vert->z;
	x1 = f->edge->next->vert->x;
	y1 = f->edge->next->vert->y;
	z1 = f->edge->next->vert->z;
	x2 = f->edge->prev->vert->x;
	y2 = f->edge->prev->vert->y;
	z2 = f->edge->prev->vert->z;

	a = (y1 - y0)*(z2 - z0) - (y2 - y0)*(z1 - z0);
	b = (z1 - z0)*(x2 - x0) - (z2 - z0)*(x1 - x0);
	c = (x1 - x0)*(y2 - y0) - (x2 - x0)*(y1 - y0);
	d = sqrt(a*a + b*b + c*c);

	f->nx = a / d;
	f->ny = b / d;
	f->nz = c / d;
	f->area = d;

}

void v_norm(HE_vert* v) {

	HE_edge* outgoing_he = v->edge;
	HE_edge* curr = outgoing_he;
	vector<float> a, b, c, d;
	unsigned int i;
	float x = 0, y = 0, z = 0, sum = 0;

	while (curr->pair&&curr->pair->next != outgoing_he) {
		a.push_back(curr->face->nx);
		b.push_back(curr->face->ny);
		c.push_back(curr->face->nz);
		d.push_back(curr->face->area);
		sum += curr->face->area;
		curr = curr->pair->next;
	}

	if (!curr->pair) {
		curr = outgoing_he;
		while (curr->prev->pair&&curr->prev->pair != outgoing_he) {
			a.push_back(curr->face->nx);
			b.push_back(curr->face->ny);
			c.push_back(curr->face->nz);
			d.push_back(curr->face->area);
			sum += curr->face->area;
			curr = curr->prev->pair;
		}
	}

	for (i = 0; i<a.size(); i++) {
		x += a[i] * d[i] / sum;
		y += b[i] * d[i] / sum;
		z += c[i] * d[i] / sum;
	}

	v->nx = x;
	v->ny = y;
	v->nz = z;
	vector<float>().swap(a);
	vector<float>().swap(b);
	vector<float>().swap(c);
	vector<float>().swap(d);
}
#pragma endregion

int read_file(string filename)
{
	// load the mesh from a file
	vector<HE_vert*>().swap(he_vert);
	vector<HE_edge*>().swap(he_edge);
	vector<HE_face*>().swap(he_face);

	ifstream myfile(filename);
	float xmin = 0., xmax = 0., ymin = 0., ymax = 0., zmin = 0., zmax = 0.;

	if (!myfile.is_open()) {
		cout << "Fail to open the file.\n";
		return 0;
	}  // fail to open the file

	string line;
	int n, v1, v2, v3;

	while (!myfile.eof()) {
		myfile >> line;

		//save vertex
		if (line == "Vertex") {
			HE_vert* he_v = new HE_vert();
			myfile >> n >> he_v->x >> he_v->y >> he_v->z;
			he_vert.push_back(he_v);
			//find boundary
			if (xmin > he_v->x)  xmin = he_v->x;
			if (xmax < he_v->x) xmax = he_v->x;
			if (ymin > he_v->y)  ymin = he_v->y;
			if (ymax < he_v->y) ymax = he_v->y;
			if (zmin > he_v->z)  zmin = he_v->z;
			if (zmax < he_v->z) zmax = he_v->z;
		}

		if (line == "Face") {

			myfile >> n >> v1 >> v2 >> v3;

			HE_face* he_f = new HE_face();
			HE_edge *he_e1 = new HE_edge(), *he_e2 = new HE_edge(), *he_e3 = new HE_edge();

			he_e1->vert = he_vert[v1 - 1];
			he_e2->vert = he_vert[v2 - 1];
			he_e3->vert = he_vert[v3 - 1];

			he_e2->next = he_e1->prev = he_e3;
			he_e1->next = he_e3->prev = he_e2;
			he_e3->next = he_e2->prev = he_e1;

			he_e1->face = he_e2->face = he_e3->face = he_f;
			he_f->edge = he_e1;

			he_edge.push_back(he_e1);
			he_edge.push_back(he_e2);
			he_edge.push_back(he_e3);
			he_face.push_back(he_f);

			//save all outgoing edges for each vertex
			if (!he_vert[v2 - 1]->edge) {
				he_vert[v2 - 1]->edge = he_e3;
				he_vert[v2 - 1]->temp.push_back(he_e3);
			}
			else he_vert[v2 - 1]->temp.push_back(he_e3);

			if (!he_vert[v1 - 1]->edge) {
				he_vert[v1 - 1]->edge = he_e2;
				he_vert[v1 - 1]->temp.push_back(he_e2);
			}
			else he_vert[v1 - 1]->temp.push_back(he_e2);

			if (!he_vert[v3 - 1]->edge) {
				he_vert[v3 - 1]->edge = he_e1;
				he_vert[v3 - 1]->temp.push_back(he_e1);
			}
			else he_vert[v3 - 1]->temp.push_back(he_e1);
		}

	}
	myfile.close();

	//calculate position and scale
	px = (xmax + xmin) / 2;
	py = (ymax + ymin) / 2;
	pz = (zmax + zmin) / 2;

	obj_scale = (float)(1 / pow((zmax - zmin)*(ymax - ymin)* (xmax - xmin), 1.0 / 3));
	x_scale = (xmax - xmin) *obj_scale;
	y_scale = (ymax - ymin)*obj_scale;
	z_scale = (zmax - zmin)*obj_scale;

	//pair up half edges
	vector<HE_edge*>::iterator it1, it2;

	for (it1 = he_edge.begin(); it1 != he_edge.end(); it1++) {
		if ((*it1)->pair) continue;
		for (it2 = (*it1)->vert->temp.begin(); it2 != (*it1)->vert->temp.end(); it2++) {
			if ((*it2)->vert == (*it1)->prev->vert) {
				(*it1)->pair = (*it2);
				(*it2)->pair = (*it1);
				break;
			}
		}
	}

	//compute face normal
	vector<HE_face*>::iterator it3;
	for (it3 = he_face.begin(); it3 != he_face.end(); it3++) {
		f_norm(*it3);
	}

	//clear templist and compute vertex normal
	vector<HE_vert*>::iterator it4;
	for (it4 = he_vert.begin(); it4 != he_vert.end(); it4++) {
		vector<HE_edge*>().swap((*it4)->temp);
		v_norm(*it4);
	}

	return 1;
}

#pragma region drawing function
void draw_wireframe() {
	vector<HE_edge*>::iterator it;

	glPushMatrix();
	glScalef(obj_scale, obj_scale, obj_scale);
	glTranslatef(-px, -py, -pz);
	glBegin(GL_LINES);
	for (it = he_edge.begin(); it != he_edge.end(); it++) {
		if ((*it)->flag) {
			glVertex3f((*it)->vert->x, (*it)->vert->y, (*it)->vert->z);
			glVertex3f((*it)->prev->vert->x, (*it)->prev->vert->y, (*it)->prev->vert->z);
			if ((*it)->pair) (*it)->pair->flag = 0;
		}
	}
	glEnd();
	glPopMatrix();
}

void draw_flat() {

	vector<HE_face*>::iterator it;
	HE_edge* pt;
	int i;

	glShadeModel(GL_FLAT);
	glPushMatrix();
	glScalef(obj_scale, obj_scale, obj_scale);
	glTranslatef(-px, -py, -pz);
	glBegin(GL_TRIANGLES);
	for (it = he_face.begin(); it != he_face.end(); it++) {
		
		for (i = 0, pt = (*it)->edge; i < 3; i++) {
			glNormal3f(pt->vert->nx, pt->vert->ny, pt->vert->nz);
			glVertex3f(pt->vert->x, pt->vert->y, pt->vert->z);
			pt = pt->next;
		}
		
	}
	glEnd();
	glPopMatrix();
}

void draw_smooth() {
	vector<HE_face*>::iterator it;
	HE_edge* pt;
	int i;

	glShadeModel(GL_SMOOTH);
	glPushMatrix();
	glScalef(obj_scale, obj_scale, obj_scale);
	glTranslatef(-px, -py, -pz);
	glBegin(GL_TRIANGLES);
	for (it = he_face.begin(); it != he_face.end(); it++) {

		for (i = 0, pt = (*it)->edge; i < 3; i++) {
			glNormal3f(pt->vert->nx, pt->vert->ny, pt->vert->nz);
			glVertex3f(pt->vert->x, pt->vert->y, pt->vert->z);
			pt = pt->next;
		}

	}
	glEnd();
	glPopMatrix();
}

void draw_points() {
	vector<HE_vert*>::iterator it;

	glPushMatrix();
	glScalef(obj_scale, obj_scale, obj_scale);
	glTranslatef(-px, -py, -pz);
	glBegin(GL_POINTS);
	for (it = he_vert.begin(); it != he_vert.end(); it++) {
		glVertex3f((*it)->x, (*it)->y, (*it)->z);
	}
	glEnd();
	glPopMatrix();
}

void draw_mesh() {
	//draw mesh

	if (obj_mode == OBJ_WIREFRAME)	  draw_wireframe();
	else if (obj_mode == OBJ_FLAT)	  draw_flat();
	else if (obj_mode == OBJ_POINTS)	  draw_points();
	else if (obj_mode == OBJ_SMOOTH)	  draw_smooth();

}

void draw_z_axis(GLUquadric *z) {

	glPushMatrix();
	glTranslatef(0, 0, 3);
	glutSolidCone(.03, .5, 10, 1);
	glPopMatrix();
	gluCylinder(z, .015, .015, 3, 10, 1);
}

void draw_y_axis(GLUquadric *y) {

	glPushMatrix();
	glRotatef(-90, 1, 0, 0);
	draw_z_axis(y);
	glPopMatrix();
}

void draw_x_axis(GLUquadric *x) {

	glPushMatrix();
	glRotatef(90, 0, 1, 0);
	draw_z_axis(x);
	glPopMatrix();
}

void draw_axes()
{
	glDisable(GL_LIGHTING);

	//lPushMatrix();
	//glScalef( scale, scale, scale );

	GLUquadric *z = gluNewQuadric(), *y = gluNewQuadric(), *x = gluNewQuadric();
	glColor3f(1, 0, 0);
	draw_z_axis(z);
	glColor3f(0, 1, 0);
	draw_y_axis(y);
	glColor3f(0, 0, 1);
	draw_x_axis(x);

	//glPopMatrix();

	glEnable(GL_LIGHTING);
}

void draw_ground() {
	float i;

	glDisable(GL_LIGHTING);

	glColor3f(1., 1., 1.);
	glBegin(GL_LINES);
	for (i = -10.; i <= 10.; i++) {
		glVertex3f(i, 0., -10.);
		glVertex3f(i, 0., 10.);
		glVertex3f(-10., 0., i);
		glVertex3f(10., 0., i);
	}
	glEnd();

	glEnable(GL_LIGHTING);
}

void draw_bb() {

	glDisable(GL_LIGHTING);

	glColor3f(0.5, 0.5, 0.5);
	glPushMatrix();
	glScalef(x_scale, y_scale, z_scale);
	glutWireCube(1.0);     // display the bounding box
	glPopMatrix();

	glEnable(GL_LIGHTING);
}
#pragma endregion

#pragma region projection setting

void setup_light() {
	glLightfv(GL_LIGHT0, GL_POSITION, light_p);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, light_d);
	glLightfv(GL_LIGHT0, GL_SPECULAR, light_s);
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lmodel_ambient);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	// normalize normal vectors
	glEnable(GL_NORMALIZE);
}

void setup_mat() {
	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, mat_a);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, mat_d);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat_s);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, mat_shininess);
}

void set_persp() {
	rotate_on = 1;
	gluPerspective(60., 1., .1, 100.);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(5., 5., 5., 0., 0., 0., 0., 1., 0.);
}

void set_XY() {
	rotate_on = 0;
	glOrtho(-5., 5., -5., 5., -10., 10.);   // set an orthogonal projection
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(0., 0., 5., 0., 0., 0., 0., 1., 0.);
	x_angle = 0;
	y_angle = 0;
}

void set_YZ() {
	rotate_on = 0;
	glOrtho(-5., 5., -5., 5., -10., 10.);   // set an orthogonal projection
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(5., 0., 0., 0., 0., 0., 0., 1., 0.);
	x_angle = 0;
	y_angle = 0;
}

void set_XZ() {
	rotate_on = 0;
	glOrtho(-5., 5., -5., 5., -10., 10.);   // set an orthogonal projection
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(0., 5., 0., 0., 0., 0., 0., 0., 1.);
	x_angle = 0;
	y_angle = 0;
}

void set_proj() {
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	if (proj_mode == VIEW_PERSP) set_persp();
	else if (proj_mode == VIEW_XY) set_XY();
	else if (proj_mode == VIEW_XZ) set_XZ();
	else if (proj_mode == VIEW_YZ) set_YZ();
}
#pragma endregion

void control_cb(int control)
{
	string file_name="";

	if (control == FILE_BROWSER) {
		file_name = fb->get_file();

		read_file(file_name);
		show_mesh = 1;
		glutPostRedisplay();

	}
}

void myDisplay( void )
{
 
	glClearColor( .8f, .8f, .8f, 1.f );
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	  
	// setup the projection
  	set_proj();

  	// rotate and scale the object
 	glTranslatef(tx, -ty, 0);
	glRotatef(x_angle, 0, 1, 0);
	glRotatef(y_angle, 1, 0, 0);
	glScalef(scale_size, scale_size, scale_size);

	if(show_ground)  draw_ground();
	if (show_axes) draw_axes(); 
	if (show_mesh) draw_mesh();
	if (show_bb) draw_bb();

	glutSwapBuffers(); 
}

#pragma region camera control
void mymouse(int button, int state, int x, int y)
{
	if (state == GLUT_DOWN)
	{
		press_x = x; press_y = y;
		if (button == GLUT_LEFT_BUTTON)
			xform_mode = TRANSFORM_ROTATE;
		else if (button == GLUT_RIGHT_BUTTON)
			xform_mode = TRANSFORM_SCALE;
		else if (button == GLUT_MIDDLE_BUTTON)
			xform_mode = TRANSFORM_TRANSLATE;
	}
	else if (state == GLUT_UP)
	{
		xform_mode = TRANSFORM_NONE;
	}
}

void mymotion(int x, int y)
{
	if (rotate_on&&xform_mode == TRANSFORM_ROTATE)
	{
		x_angle += (float)((x - press_x) / 5.0);

		if (x_angle > 180)
			x_angle -= 360;
		else if (x_angle <-180)
			x_angle += 360;

		press_x = x;

		y_angle += (float)((y - press_y) / 5.0);

		if (y_angle > 180)
			y_angle -= 360;
		else if (y_angle <-180)
			y_angle += 360;

		press_y = y;
	}
	else if (xform_mode == TRANSFORM_SCALE)
	{
		float old_size = scale_size;

		scale_size *= (float)(1 + (y - press_y) / 60.0);

		if (scale_size <0)
			scale_size = old_size;
		press_y = y;
	}
	else if (xform_mode == TRANSFORM_TRANSLATE)
	{
		tx += (float)((x - press_x) / 60.0);
		ty += (float)((y - press_y) / 60.0);
		press_x = x;
		press_y = y;
	}
	// force the redraw function
	glutPostRedisplay();
}
#pragma endregion

int main(int argc, char* argv[])
{
 	//Initialize and create window
	glutInit(&argc, argv);
	glutInitDisplayMode( GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH );
	glutInitWindowPosition( 50, 50 );
	glutInitWindowSize( winW, winH );
 
	main_window = glutCreateWindow( "Mesh Viewer (ZHAO TIANYE)" );
	glutDisplayFunc( myDisplay );
	glutMotionFunc(mymotion);
	glutMouseFunc(mymouse);

	setup_light();
	setup_mat();
	glEnable(GL_DEPTH_TEST);

  	//Create subwindow
  	glui = GLUI_Master.create_glui_subwindow( main_window,GLUI_SUBWINDOW_RIGHT );
	
	obj_panel = new GLUI_Panel(glui, "",true);
	new GLUI_StaticText(obj_panel, "Open File:");
  	fb = new GLUI_FileBrowser(obj_panel, "", false, FILE_BROWSER, control_cb);

	obj_panel = new GLUI_Panel(glui, "Projecton:", true);
  	radio = new GLUI_RadioGroup(obj_panel, &proj_mode, 3, control_cb);
  	new GLUI_RadioButton(radio, "X-Y Plane");
	new GLUI_RadioButton(radio, "Y-Z Plane");
	new GLUI_RadioButton(radio, "X-Z Plane");
  	new GLUI_RadioButton(radio, "Perspective");
    
	obj_panel = new GLUI_Panel(glui, "Rendering:", true);
  	radio = new GLUI_RadioGroup(obj_panel, &obj_mode, 4, control_cb);
 	new GLUI_RadioButton(radio, "Points" );
  	new GLUI_RadioButton(radio, "Wireframe");
 	new GLUI_RadioButton(radio, "Flat");
  	new GLUI_RadioButton(radio, "Smooth");
	  
	obj_panel = new GLUI_Panel(glui, "Options:", true);
 	new GLUI_Checkbox(obj_panel, "Ground", &show_ground );
 	new GLUI_Checkbox(obj_panel, "Axes", &show_axes );
 	new GLUI_Checkbox(obj_panel, "Bounding Box", &show_bb );

  	//quit button
 	new GLUI_Button( glui, "Quit", 0,(GLUI_Update_CB)exit );
	
	//Link windows to GLUI
  	glui->set_main_gfx_window( main_window );
	
	//GLUT main loop
  	glutMainLoop();

 	return EXIT_SUCCESS;
}