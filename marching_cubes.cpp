// include required libraries
#include <pcl/io/io.h>
#include <pcl/io/pcd_io.h>
#include "marching_cubes_table.h"
#include <pcl/visualization/cloud_viewer.h>
#include <pcl/geometry/triangle_mesh.h>


struct Triange {
    pcl::PointXYZ vert[3];
} ;
struct Cell {
    pcl::PointXYZ vert[8];
    float val[8];
} ;
int count = 0;
float isolevel = 0;

/*
   Linearly interpolate the position where an isosurface cuts
   an edge between two vertices, each with their own scalar value
*/
pcl::PointXYZ VertexInterp(float isolevel,pcl::PointXYZ p1,pcl::PointXYZ p2,float valp1,float valp2) {
    float mu;
    pcl::PointXYZ p;

    // If jump is too big, return a point with x index -1 to indicate it's false
    if (std::abs(valp1-valp2) > 0.8){
        p.x = -1;
        return(p);
    }

    if (std::abs(isolevel-valp1) < 0.00001){
        return(p1);
    }
    if (std::abs(isolevel-valp2) < 0.00001){
        return(p2);
    }
    if (std::abs(valp1-valp2) < 0.00001){
        return(p1);
    }
    mu = (isolevel - valp1) / (valp2 - valp1);
    p.x = p1.x + mu * (p2.x - p1.x);
    p.y = p1.y + mu * (p2.y - p1.y);
    p.z = p1.z + mu * (p2.z - p1.z);
    return(p);
}


int process_cube(Cell grid, pcl::PointCloud<pcl::PointXYZ>::Ptr cloud) {
    int cubeindex = 0;
    if (grid.val[0] <= isolevel) cubeindex |= 1;
    if (grid.val[1] <= isolevel) cubeindex |= 2;
    if (grid.val[2] <= isolevel) cubeindex |= 4;
    if (grid.val[3] <= isolevel) cubeindex |= 8;
    if (grid.val[4] <= isolevel) cubeindex |= 16;
    if (grid.val[5] <= isolevel) cubeindex |= 32;
    if (grid.val[6] <= isolevel) cubeindex |= 64;
    if (grid.val[7] <= isolevel) cubeindex |= 128;

    // Cube is entirely in/out of the surface or has value 0
    if (edgeTable[cubeindex] == 0){
        return(0);
    }
    pcl::PointXYZ vertlist[12];

    // Find the points where the surface intersects the cube
    if (edgeTable[cubeindex] & 1){
        vertlist[0] = VertexInterp(isolevel,grid.vert[0],grid.vert[1],grid.val[0],grid.val[1]);
    }
    if (edgeTable[cubeindex] & 2){
        vertlist[1] = VertexInterp(isolevel,grid.vert[1],grid.vert[2],grid.val[1],grid.val[2]);
    }
    if (edgeTable[cubeindex] & 4){
        vertlist[2] = VertexInterp(isolevel,grid.vert[2],grid.vert[3],grid.val[2],grid.val[3]);
    }
    if (edgeTable[cubeindex] & 8){
        vertlist[3] = VertexInterp(isolevel,grid.vert[3],grid.vert[0],grid.val[3],grid.val[0]);
    }
    if (edgeTable[cubeindex] & 16){
        vertlist[4] = VertexInterp(isolevel,grid.vert[4],grid.vert[5],grid.val[4],grid.val[5]);
    }
    if (edgeTable[cubeindex] & 32){
        vertlist[5] = VertexInterp(isolevel,grid.vert[5],grid.vert[6],grid.val[5],grid.val[6]);
    }
    if (edgeTable[cubeindex] & 64){
        vertlist[6] = VertexInterp(isolevel,grid.vert[6],grid.vert[7],grid.val[6],grid.val[7]);
    }
    if (edgeTable[cubeindex] & 128){
        vertlist[7] = VertexInterp(isolevel,grid.vert[7],grid.vert[4],grid.val[7],grid.val[4]);
    }
    if (edgeTable[cubeindex] & 256){
        vertlist[8] = VertexInterp(isolevel,grid.vert[0],grid.vert[4],grid.val[0],grid.val[4]);
    }
    if (edgeTable[cubeindex] & 512){
        vertlist[9] = VertexInterp(isolevel,grid.vert[1],grid.vert[5],grid.val[1],grid.val[5]);
    }
    if (edgeTable[cubeindex] & 1024){
        vertlist[10] = VertexInterp(isolevel,grid.vert[2],grid.vert[6],grid.val[2],grid.val[6]);
    }
    if (edgeTable[cubeindex] & 2048){
        vertlist[11] = VertexInterp(isolevel,grid.vert[3],grid.vert[7],grid.val[3],grid.val[7]);
    }
    for(int i=0;i<12;i++){
        if(vertlist[i].x < 0){
            return(0);
        }
    }
    count+=1;
    // Create the triangle
    int triangle_count = 0;
    pcl::PointXYZ triangle[3];
    for (int i=0;triTable[cubeindex][i]!=-1;i+=3) {
        triangle[0] = vertlist[triTable[cubeindex][i  ]];
        triangle[1] = vertlist[triTable[cubeindex][i+1]];
        triangle[2] = vertlist[triTable[cubeindex][i+2]];
        cloud->push_back(triangle[0]);
        cloud->push_back(triangle[1]);
        cloud->push_back(triangle[2]);
        triangle_count++;
    }

    return(triangle_count);

}

int main (int argc, char * argv[])
{
    std::string tsdfDirectory = ".";
    if (argc != 2) {
        std::cout << "usage: ./tsdf <TSDF binary file directory>. File should be named tsdf.bin. Default is current folder."
                  << std::endl;
    } else {
        tsdfDirectory = std::string(argv[1]);
    }
    std::string tsdfName = tsdfDirectory + "/tsdf.bin";
    FILE * fp = fopen(tsdfName.c_str(), "r");
    if(!fp){
        std::cerr << "File not found" << std::endl;
        return 1;
    }
    std::cout << "File found. Processing..." << std::endl;
    std::cout << "Implementing Marching Cubes Algorithm..." << std::endl;
    std::cout.flush();

    // The scene is pretty small so recommended size is 1.
    int size = 1;
    float tsdf_value = 0;
    pcl::PointCloud<pcl::PointXYZ> cloud;
    pcl::PointCloud<pcl::PointXYZ>::Ptr ptrCloud(&cloud);
    pcl::PolygonMesh  triangleMesh;
    pcl::PolygonMesh::Ptr ptrMesh(&triangleMesh);
    struct Cell gridcell;
    double vert[8][3];
    float val[8];


    // Calculate gridcell as long as the first size layers are scanned to reduce memory usage
    float points_arr[2][512/size][512/size];
    for (int i = 0; i < 512; i++) {
        if(i == 0){
            // Only if i=0: scan the first layer
            for (int j = 0; j < 512; j++) {
                for (int k = 0; k < 512; k++) {
                    if(fread((void*)(&tsdf_value), sizeof(tsdf_value), 1, fp)) {
                        if(j % size == 0 && k % size==0){
                            points_arr[0][j/size][k/size] = tsdf_value;
                        }
                    }
                }
            }
        }
        else if(i%size==0){
            std::cout << "Scanning layer " << i << "/" << 512-size <<"... \r";
            std::cout.flush();
            // scan the second layer
            for (int j = 0; j < 512; j++) {
                for (int k = 0; k < 512; k++) {
                    if(fread((void*)(&tsdf_value), sizeof(tsdf_value), 1, fp)) {
                        if( j%size == 0 && k%size==0){
                            points_arr[1][j/size][k/size] = tsdf_value;
                        }
                    }
                }
            }
            for(int j=0;j < 512/size - 1;j++){
                for(int k=0; k < 512/size - 1;k++){
                    // Retrieve 8 vertices of the gridcell. Order is the default order from the paper.
                    int index_arr[8][3] = { {0,j,k+1},
                                            {1,j,k+1},
                                            {1,j,k},
                                            {0,j,k},
                                            {0,j+1,k+1},
                                            {1,j+1,k+1},
                                            {1,j+1,k},
                                            {0,j+1,k} };
                    int position_arr[8][3] = { {i-size,j*size,k*size+size},
                                            {i,j*size,k*size+size},
                                            {i,j*size,k*size},
                                            {i-size,j*size,k*size},
                                            {i-size,j*size+size,k*size+size},
                                            {i,j*size+size,k*size+size},
                                            {i,j*size+size,k*size},
                                            {i-size,j*size+size,k*size} };
                    for(int l=0;l<8;l++){
                        gridcell.val[l] = points_arr[index_arr[l][0]][index_arr[l][1]][index_arr[l][2]];
                        gridcell.vert[l].x = position_arr[l][0];
                        gridcell.vert[l].y = position_arr[l][1];
                        gridcell.vert[l].z = position_arr[l][2];
                    }
                    process_cube(gridcell,ptrCloud);
                    /**
                    if(i==4 && j==50 && k==52){
                        for(int m = 0; m < 8; m++) {
                           printf("%f ", gridcell.val[m]);
                        }
                        printf("\n");
                        for(int m = 0; m < 8; m++) {
                            for(int n = 0; n < 3; n++) {
                                printf("%d ", index_arr[m][n]);
                            }
                            printf("\n");
                        }
                        printf("\n");
                        for(int m = 0; m < 8; m++) {
                            for(int n = 0; n < 3; n++) {
                                printf("%d ", position_arr[m][n]);
                            }
                            printf("\n");
                        }
                    }
                    **/
                }
            }
            // Move second layer to first layer
            for (int j = 0; j < 512/size; j++) {
                for (int k = 0; k < 512/size; k++) {
                    points_arr[0][j][k] = points_arr[1][j][k] ;
                }
            }
        }
    }
    std::cout << "A total of " << count << " cubes are processed." << std::endl;
    std::cout << cloud.size() << std::endl;
    pcl::visualization::CloudViewer viewer("Cloud Viewer");
    viewer.showCloud(ptrCloud);
    while (!viewer.wasStopped ())
    {

    }
    return 0;
}
