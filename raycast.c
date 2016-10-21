#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <ctype.h>

/* 	Jesus Garcia
	Project 3 - Raycasting with Illumination - 10/20/16
	CS430 - Prof. Palmer
	
	"In the previous project you will wrote code to raycast 
	mathematical primitives based on a scene
	input file into a pixel buffer. In this project you will 
	color objects based on the shading model we discussed in class."
	
	Your program (raycast) should have this usage pattern:
		raycast width height input.json output.ppm
		
	This improved program will include illumination elements...
	
	position The location of the light
	color The color of the light (vector)
	radial-a0 The lowest order term in the radial attenuation function (lights only)
	radial-a1 The middle order term in the radial attenuation function (lights only)
	radial-a2 The highest order term in the radial attenuation function (lights only)
	theta The angle of the spotlight cone (spot lights only) in degrees;
	If theta = 0 or is not present, the light is a point light;
	
	Note that the C trig functions assume radians so you may need to do
	a conversion.
	
	angular-a0 The exponent in the angular attenuation function (spot lights only)
	direction The direction vector of the spot light (spot lights only)
	If direction is not present, the light is a point light
	
	For objects, the properties from the last assignment should be supported in addition to:
		diffuse_color The diffuse color of the object (vector)
		specular_color The specular color of the object (vector)
		You	may	optionally	include	an	object property,	ns,	for	shininess.
		If	the	property	is	not	present	please	set	the	value	to 20 
		(you	may	simply	hard	code	the	value	in	the	specular	 equation).
	
*/

double line=1;

// Struct Definitions
typedef struct{
    double r, g, b;
	
}	PixData;
typedef struct{
    double height;
    double width;
	
}	Camera;
typedef struct{
    double center[3];
    double diffuse_color[3];
	double specular_color[3];
    double radius;
	
}	Sphere;
typedef struct{
    double position[3];
    double diffuse_color[3];
    double normal[3];

}	Plane;
typedef struct{
    double color[3];
	double direction[3];
	double radiala0;
	double radiala1;
	double radiala2;
	double position[3];
	double angulara0;
	double theta;

}	Light;
typedef struct{
    int objType; 
    Camera cam;		// 0
    Sphere sphere;	// 1
    Plane plane;	// 2
	Light light;	// 3
	
}	Object;

// Iterate through a file, a json in this case
int next_c(FILE* json) {
	int c = fgetc(json);
#ifdef DEBUG
	printf("next_c: '%c'\n", c);
#endif
	if (c == '\n') {
		line += 1;
	}
	if (c == EOF) {
		fprintf(stderr, "Error: Unexpected end of file on line number %d.\n", line);
		exit(1);
	}
	return c;
}


// expect_c() checks that the next character is d.  If it is not it emits
// an error.
void expect_c(FILE* json, int d) {
	int c = next_c(json);
	if (c == d) return;
	fprintf(stderr, "Error: Expected '%c' on line %d.\n", d, line);
	exit(1);
}


// skip_ws() skips white space in the file.
void skip_ws(FILE* json) {
	int c = next_c(json);
	while (isspace(c)) {
		c = next_c(json);
	}
	ungetc(c, json);
}


// next_string() gets the next string from the file handle and emits an error
// if a string can not be obtained.
char* next_string(FILE* json) {
	char buffer[129];
	int c = next_c(json);
	if (c != '"') {
		fprintf(stderr, "Error: Expected string on line %d.\n", line);
		exit(1);
	}
	c = next_c(json);
	int i = 0;
	while (c != '"') {
		if (i >= 128) {
			fprintf(stderr, "Error: Strings longer than 128 characters in length are not supported.\n");
			exit(1);
		}
		if (c == '\\') {
			fprintf(stderr, "Error: Strings with escape codes are not supported.\n");
			exit(1);
		}
		if (c < 32 || c > 126) {
			fprintf(stderr, "Error: Strings may contain only ascii characters.\n");
			exit(1);
		}
		buffer[i] = c;
		i += 1;
		c = next_c(json);
	}
	buffer[i] = 0;
	return strdup(buffer);
}

// Scans for numerical values
double next_number(FILE* json) {
	double value;
	fscanf(json, "%lf", &value);
	return value;
}

double* next_vector(FILE* json) {
	// Because we have 3 dimensions, or 3 values to store,
	// We must allocate 3 times as much memory to store the vector
	double* v = malloc(3*sizeof(double));
	expect_c(json, '[');
	skip_ws(json);
	
	v[0] = next_number(json);
	skip_ws(json);
	expect_c(json, ',');
	skip_ws(json);
	
	v[1] = next_number(json);
	skip_ws(json);
	expect_c(json, ',');
	skip_ws(json);
	
	v[2] = next_number(json);
	skip_ws(json);
	expect_c(json, ']');
	
	return v;
}

// From the given JSON parser code, modified to actually do something useful
void read_scene(char* filename, Object* object) {
	int c;
	double line;

	FILE* json = fopen(filename, "r");

	if (json == NULL) {
		fprintf(stderr, "Error: Could not open file \"%s\"\n", filename);
		exit(1);
	}

	skip_ws(json);

	// Find the beginning of the list
	expect_c(json, '[');

	skip_ws(json);

	// Find the objects
	
	int i = 0;

	while (1) {
		// Initialization of possible objects in the JSON
		Camera cam;
		Sphere sphere;
		Plane plane;
		Light light;
		line += 1.0;
		
		c = fgetc(json);
		// This if statement trigger would mean the json contains basically nothing
		if (c == ']') {
			fprintf(stderr, "Error: This is the worst scene file EVER.\n");
			fclose(json);
			return;
		}
		
		if (c == '{') {
			skip_ws(json);

			// Parse the object
			char* key = next_string(json);
			if (strcmp(key, "type") != 0) {
				fprintf(stderr, "Error: Expected \"type\" key on line number %d.\n", line);
				exit(1);
			}

			// Iterating through the 'junk' spaces of JSON files
			skip_ws(json);
			expect_c(json, ':');
			skip_ws(json);
			
			// Store a useful value once we get to it
			char* value = next_string(json);

			// Here we find the object type fields and store them in the object array
			if (strcmp(value, "camera") == 0) {
				object[i].objType=0;
			} else if (strcmp(value, "sphere") == 0) {
				object[i].objType = 1;
			} else if (strcmp(value, "plane") == 0) {
				object[i].objType = 2;
			} else if (strcmp(value, "light") == 0) {
				object[i].objType = 3;
			} else {
				fprintf(stderr, "Error: Unknown type, \"%s\", on line number %d.\n", value, line);
				exit(1);
			}

			skip_ws(json);

			while (1) {
				
				// , }
				c = next_c(json);
				if (c == '}') {
					// stop parsing this object
					break;
				} else if (c == ',') {
					// read another field
					skip_ws(json);
					char* key = next_string(json);
					skip_ws(json);
					expect_c(json, ':');
					skip_ws(json);
					
		if ((strcmp(key, "width") == 0) || (strcmp(key, "height") == 0) || (strcmp(key, "radius") == 0) || (strcmp(key, "radial-a0") == 0) || (strcmp(key, "radial-a1") == 0) || (strcmp(key, "radial-a2") == 0)) {
						double value = next_number(json);
						// Width in the json means its the camera object
						if (strcmp(key, "width")==0) {
							cam.width=value;
						// Similiarly, height will mean its the camera object as well
						} else if (strcmp(key, "height") == 0) {
							cam.height=value;
						// Once again, radius is exclusive to circle objects
						} else if (strcmp(key, "radius") == 0) {
							sphere.radius=value;
						}
					} else if ((strcmp(key, "color") == 0) || (strcmp(key, "diffuse_color") == 0) || (strcmp(key, "specular_color") == 0) || (strcmp(key, "position") == 0) || (strcmp(key, "normal") == 0)) {
						// Depending on the position in the object array, this is store the
						// Color components in the given object data
						double* value = next_vector(json);
						if (strcmp(key, "diffuse_color") == 0) {
							if (object[i].objType == 1) {
								sphere.diffuse_color[0] = value[0]; sphere.diffuse_color[1] = value[1]; sphere.diffuse_color[2] = value[2];
							}
							if (object[i].objType == 2) {
								plane.diffuse_color[0] = value[0]; plane.diffuse_color[1] = value[1]; plane.diffuse_color[2] = value[2];
							}
						}
						if (strcmp(key, "specular_color") == 0) {
							if (object[i].objType == 1) {
								sphere.specular_color[0] = value[0]; sphere.specular_color[1] = value[1]; sphere.specular_color[2] = value[2];
							}
						}
						if (strcmp(key, "color") == 0) {
							if (object[i].objType == 3) {
								light.color[0] = value[0]; light.color[1] = value[1]; light.color[2] = value[2];
							}
						}
						if (strcmp(key, "position") == 0) {
							// If parsed object is...
								// Sphere
							if (object[i].objType == 1) {
								sphere.center[0] = value[0]; sphere.center[1]  = value[1]; sphere.center[2]  = value[2];
							}
								// Plane
							if (object[i].objType == 2) {
								plane.position[0]  = value[0]; plane.position[1]  = value[1]; plane.position[2]  = value[2];
							}
							if (object[i].objType == 3) {
								light.position[0]  = value[0]; light.position[1]  = value[1]; light.position[2]  = value[2];
							}
						}
						// Normal is purely a 'plane' attribute so we only check for plane objType
						if (strcmp(key, "normal") == 0) {
							if (object[i].objType == 2) {
								plane.normal[0]  = value[0]; plane.normal[1]  = value[1]; plane.normal[2]  = value[2];
							}
						}
						// If the property is undefined for this project
					} else {
						fprintf(stderr, "Error: Unknown property, \"%s\", on line %d.\n", key, line);
					}
					skip_ws(json);
				// If values aren't properly split up through comma's
				} else {
					fprintf(stderr, "Error: Unexpected value on line %d\n", line);
					exit(1);
				}
				// Store the final objects in the loop iteration
				object[i].cam=cam;
				object[i].sphere=sphere;
				object[i].plane=plane;
				object[i].light=light;
			}
			
			skip_ws(json);
			c = next_c(json);
			
			if (c == ',') {
				// noop
				skip_ws(json);
			// Waiting for the EOF of the JSON
			} else if (c == ']') {
				fclose(json);
				return;
			} else {
				fprintf(stderr, "Error: Expecting ',' or ']' on line %d.\n", line);
				exit(1);
			}
		}
		// This will iterate the position in the object array
		// We will continue on until the return is hit when we hit the EOF
		i++;
	}
}

// Some basic math functions
static inline double sqr(double num){ return num*num; }
static inline void normalize(double* v){ double Rd = sqr(v[0])+sqr(v[1])+sqr(v[2]); }


void makeP3PPM(PixData* pixData, int width, int height, char* filename){

	// Opening the file to be written
    FILE *fh = fopen(filename,"w+");
	
	// Writing the P3 ppm Header
    fprintf(fh, "P3\n");
	fprintf(fh, "# Jesus Garcia Project 3\n");
	fprintf(fh, "%d %d\n", width, height);
	fprintf(fh, "255\n");
	
	// Writing P3 pixel data
    for(int j = 0; j< width*height; j++){
        fprintf(fh, "%d ", pixData[j].r);
        fprintf(fh, "%d ", pixData[j].g);
        fprintf(fh, "%d \n", pixData[j].b);
    }
	
	// Close file
    fclose(fh);
}

// Finding where, if it exists, sphere intersection
double sphere_insxion(double* Ro, double* Rd, double rad, double* center ){

    // Mathematical functions as given by the Raycasting pdf
    double alpha = sqr(Rd[0]) + sqr(Rd[1]) + sqr(Rd[2]);
    double beta = 2 * (Rd[0] * (Ro[0]-center[0]) + Rd[1] * (Ro[1]-center[1]) + Rd[2] * (Ro[2]-center[2]));
    double gamma = sqr(Ro[0]-center[0]) + sqr(Ro[1]-center[1]) + sqr(Ro[2]-center[2]) - sqr(rad);

	// Finding Determinant
    double det = sqr(beta) - 4 * alpha * gamma;

	// Error check if there are no intersections
    if (det < 0) { return -1; }
    det = sqrt(det);

    // Quadratic equations can be + or - so we need both answers
    double t0 = (-beta - det) / (2 * alpha);// -
    if (t0 > 0){ return t0; }
    double t1 = (-beta + det) / (2 * alpha);// +
    if (t1 > 0){ return t1; }

	// Else: Return an error code (-1)
    return -1;
}


double plane_insxion(double* Ro, double* Rd, double* normal, double* position){
    // Mathematical functions as given by the Raycasting pdf
    double dist = -(normal[0] * position[0] + normal[1] * position[1] + normal[2] * position[2]);
    double t = -(normal[0] * Ro[0] + normal[1] * Ro[1] + normal[2] * Ro[2] + dist) /
                (normal[0] * Rd[0] + normal[1] * Rd[1] + normal[2] * Rd[2]);

    if (t > 0){ return t; }

	// If there is no intersection, return error code (-1)
    return -1;
}

// Error checking function to minimize code in main()
int errCheck(int args, char *argv[]){
	
	// Initial check to see if there are 3 input arguments on launch
	if ((args != 5) || (strlen(argv[3]) <= 5) || (strlen(argv[4]) <= 4)) {
		fprintf(stderr, "Error: Program requires usage: 'raycast width height input.json output.ppm'");
		exit(1);
	}

	// Check the file extension of input and output files
	char *extIn;
	char *extOut;
	if(strrchr(argv[3],'.') != NULL){
		extIn = strrchr(argv[3],'.');
	}
	if(strrchr(argv[4],'.') != NULL){
		extOut = strrchr(argv[4],'.');
	}
	
	// Check to see if the inputfile is in .ppm format
	if(strcmp(extIn, ".json") != 0){
		printf("Error: Input file not a json");
		exit(1);
	}
	
	// Check to see if the outputfile is in .ppm format
	if(strcmp(extOut, ".ppm") != 0){
		printf("Error: Output file not a PPM");
		exit(1);
	}

	return(0);
}

// Clamp function required to keep number below 255 or non-negative
double clamp(double color){
	if (color > 1.0) { color = 1.0; }
	else if (color < 0.0) { color = 0.0; }
	double clamped = 255.0 * color;
	return clamped;
}

// DONE
double frad(double lDist, double ra0, double ra1, double ra2){
		return (  (1/(ra2*sqr(lDist)) + (ra1*lDist) + ra0)  );
}
double fang(double* lDir, double anga0, double* rayVect){
	// Check if the light is a spotlight
	if((lDir[0] == 0) && (lDir[1] == 0) && (lDir[2] == 0)){
		return 1.0;
	}
	return (pow(((rayVect[0]*lDir[0]) + (rayVect[1]*lDir[1]) + (rayVect[2]*lDir[2])),anga0));
	
}

double diff_reflect(double dFactor, double lColor, double dColor){
	return (dFactor * lColor * dcolor);
}

double spec_reflect(double sFactor, double lColor, double sColor){
	// Hardcoded in from lecture instructions
	int spec_power = 20;
	return (lColor * dcolor * pow(sFactor, spec_power));
}

void illuminate(double* Ro, double* Rd, double idealT, Object idealObject, int index){
	
}

// Creation function for the image
void raycast(Object* objects,char* picture_height,char* picture_width,char* output_file){

	// Basic loop varibles
    int j,y,index=0;
	double color[3];
	
	// Change these colors later
	color[0] = 0.1;
	color[1] = 0.1;
	color[2] = 0.1;
	
	// Initialization of the camera's center
    double centerX=0;
    double centerY=0;

	// Goes through object array to find Camera
    for(j=0;j< sizeof(objects);j++){
        if(objects[j].objType == 0){ break; }
    }

	// Store the camera position
    double camH = objects[j].cam.height;
    double camW = objects[j].cam.width;
	
	// Store the desired picture height and width
    int picH = atoi(picture_height);
    int picW = atoi(picture_width);
	
	// Get the relative size of each pixel to be drawn
    double pixheight =camH/picH;
    double pixwidth =camW/picH;

	// Initialization of the pixel data for the ppm file to be drawn
    PixData pixels[picH*picW];

    for(int y=picH;y>0;y--){
        for (int x=0;x<picW;x++){
			// This will flip the picture as it looks upside down otherwise
            double Ro[3]={0,0,0};
            double Rd[3] = { centerX - (camW/2) + pixwidth * (x + 0.5), centerY - (camH/2) + pixheight * (y + 0.5), 1 };
            normalize(Rd);
            double idealInsx=INFINITY;
            Object object;

            // Loop thru the object array
            for(int i=0;i<sizeof(objects);i++) {
                double t = 0;
				
                switch (objects[i].objType) {
                    case 1:
                        t = sphere_insxion(Ro, Rd, objects[i].sphere.radius, objects[i].sphere.center);
                        break;
                    case 2:
                        t = plane_insxion(Ro, Rd, objects[i].plane.normal, objects[i].plane.position);
                        break;
                    case 0:
                        break;
                    default:
						fprintf(stderr, "Error: Not an accepted object type");
                        exit(1);
                }

                if(t>0 && t<idealInsx){
                    idealInsx=t;
                    object=objects[i];
                }
            }

			if(idealInsx > 0 && idealInsx != INFINITY){
				
				if(object.objType==1){
					pixels[index].r = (clamp(color[0]));
					pixels[index].g = (clamp(color[1]));
					pixels[index].b = (clamp(color[2]));
				}
				if(object.objType==2){
					pixels[index].r = (clamp(color[0]));
					pixels[index].g = (clamp(color[1]));
					pixels[index].b = (clamp(color[2]));
				}
			} else{
				// If no intersection, paint black area
				pixels[index].r = 0;
				pixels[index].g = 0;
				pixels[index].b = 0;
			}
            index++;
        }
    }
    //writing to ppm file
    makeP3PPM(pixels, atoi(picture_height), atoi(picture_width), output_file);
}


int main(int args, char** argv) {
	
	errCheck(args, argv);
    Object objects[129];
    read_scene(argv[3],objects);
    raycast(objects,argv[1],argv[2],argv[4]);
    return 0;
}