// Created by James McClure
// Copyright 2008-2013
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <math.h>
#include <time.h>
#include <exception>      // std::exception
#include <stdexcept>

#include "common/Domain.h"
#include "common/Array.h"
#include "common/Utilities.h"
#include "common/MPI_Helpers.h"
#include "common/Communication.h"

static int MAX_BLOB_COUNT=50;

using namespace std;

inline double minmod(double &a, double &b){

	double value;

	value = a;
	if 	( a*b < 0.0)	    value=0.0;
	else if (fabs(a) > fabs(b)) value = b;

	return value;
}

inline double Eikonal(DoubleArray &Distance, char *ID, Domain &Dm, int timesteps){

	/*
	 * This routine converts the data in the Distance array to a signed distance
	 * by solving the equation df/dt = sign(1-|grad f|), where Distance provides
	 * the values of f on the mesh associated with domain Dm
	 * It has been tested with segmented data initialized to values [-1,1]
	 * and will converge toward the signed distance to the surface bounding the associated phases
	 *
	 * Reference:
	 * Min C (2010) On reinitializing level set functions, Journal of Computational Physics	229
	 */

	int i,j,k;
	double dt=0.1;
	double Dx,Dy,Dz;
	double Dxp,Dxm,Dyp,Dym,Dzp,Dzm;
	double Dxxp,Dxxm,Dyyp,Dyym,Dzzp,Dzzm;
	double sign,norm;
	double LocalVar,GlobalVar,LocalMax,GlobalMax;

	int xdim,ydim,zdim;
	xdim=Dm.Nx-2;
	ydim=Dm.Ny-2;
	zdim=Dm.Nz-2;
	fillHalo<double> fillData(Dm.Comm, Dm.rank_info,xdim,ydim,zdim,1,1,1,0,1);

	// Arrays to store the second derivatives
	DoubleArray Dxx(Dm.Nx,Dm.Ny,Dm.Nz);
	DoubleArray Dyy(Dm.Nx,Dm.Ny,Dm.Nz);
	DoubleArray Dzz(Dm.Nx,Dm.Ny,Dm.Nz);

	int count = 0;
	while (count < timesteps){

		// Communicate the halo of values
		fillData.fill(Distance);

		// Compute second order derivatives
		for (k=1;k<Dm.Nz-1;k++){
			for (j=1;j<Dm.Ny-1;j++){
				for (i=1;i<Dm.Nx-1;i++){
					Dxx(i,j,k) = Distance(i+1,j,k) + Distance(i-1,j,k) - 2*Distance(i,j,k);
					Dyy(i,j,k) = Distance(i,j+1,k) + Distance(i,j-1,k) - 2*Distance(i,j,k);
					Dzz(i,j,k) = Distance(i,j,k+1) + Distance(i,j,k-1) - 2*Distance(i,j,k);
				}
			}
		}
		fillData.fill(Dxx);
		fillData.fill(Dyy);
		fillData.fill(Dzz);

		LocalMax=LocalVar=0.0;
		// Execute the next timestep
		for (k=1;k<Dm.Nz-1;k++){
			for (j=1;j<Dm.Ny-1;j++){
				for (i=1;i<Dm.Nx-1;i++){

					int n = k*Dm.Nx*Dm.Ny + j*Dm.Nx + i;

					sign = -1;
					if (ID[n] == 1) sign = 1;

					// local second derivative terms
					Dxxp = minmod(Dxx(i,j,k),Dxx(i+1,j,k));
					Dyyp = minmod(Dyy(i,j,k),Dyy(i,j+1,k));
					Dzzp = minmod(Dzz(i,j,k),Dzz(i,j,k+1));
					Dxxm = minmod(Dxx(i,j,k),Dxx(i-1,j,k));
					Dyym = minmod(Dyy(i,j,k),Dyy(i,j-1,k));
					Dzzm = minmod(Dzz(i,j,k),Dzz(i,j,k-1));

					/* //............Compute upwind derivatives ...................
                    Dxp = Distance(i+1,j,k) - Distance(i,j,k) + 0.5*Dxxp;
                    Dyp = Distance(i,j+1,k) - Distance(i,j,k) + 0.5*Dyyp;
                    Dzp = Distance(i,j,k+1) - Distance(i,j,k) + 0.5*Dzzp;


                    Dxm = Distance(i,j,k) - Distance(i-1,j,k) + 0.5*Dxxm;
                    Dym = Distance(i,j,k) - Distance(i,j-1,k) + 0.5*Dyym;
                    Dzm = Distance(i,j,k) - Distance(i,j,k-1) + 0.5*Dzzm;
					 */
					Dxp = Distance(i+1,j,k)- Distance(i,j,k) - 0.5*Dxxp;
					Dyp = Distance(i,j+1,k)- Distance(i,j,k) - 0.5*Dyyp;
					Dzp = Distance(i,j,k+1)- Distance(i,j,k) - 0.5*Dzzp;

					Dxm = Distance(i,j,k) - Distance(i-1,j,k) + 0.5*Dxxm;
					Dym = Distance(i,j,k) - Distance(i,j-1,k) + 0.5*Dyym;
					Dzm = Distance(i,j,k) - Distance(i,j,k-1) + 0.5*Dzzm;

					// Compute upwind derivatives for Godunov Hamiltonian
					if (sign < 0.0){
						if (Dxp + Dxm > 0.f)  	Dx = Dxp*Dxp;
						else					Dx = Dxm*Dxm;

						if (Dyp + Dym > 0.f)  	Dy = Dyp*Dyp;
						else					Dy = Dym*Dym;

						if (Dzp + Dzm > 0.f)  	Dz = Dzp*Dzp;
						else					Dz = Dzm*Dzm;
					}
					else{

						if (Dxp + Dxm < 0.f)  	Dx = Dxp*Dxp;
						else					Dx = Dxm*Dxm;

						if (Dyp + Dym < 0.f)  	Dy = Dyp*Dyp;
						else					Dy = Dym*Dym;

						if (Dzp + Dzm < 0.f)  	Dz = Dzp*Dzp;
						else					Dz = Dzm*Dzm;
					}

					//Dx = max(Dxp*Dxp,Dxm*Dxm);
					//Dy = max(Dyp*Dyp,Dym*Dym);
					//Dz = max(Dzp*Dzp,Dzm*Dzm);

					norm=sqrt(Dx + Dy + Dz);
					if (norm > 1.0) norm=1.0;

					Distance(i,j,k) += dt*sign*(1.0 - norm);
					LocalVar +=  dt*sign*(1.0 - norm);

					if (fabs(dt*sign*(1.0 - norm)) > LocalMax)
						LocalMax = fabs(dt*sign*(1.0 - norm));
				}
			}
		}

		MPI_Allreduce(&LocalVar,&GlobalVar,1,MPI_DOUBLE,MPI_SUM,Dm.Comm);
		MPI_Allreduce(&LocalMax,&GlobalMax,1,MPI_DOUBLE,MPI_MAX,Dm.Comm);
		GlobalVar /= (Dm.Nx-2)*(Dm.Ny-2)*(Dm.Nz-2)*Dm.nprocx*Dm.nprocy*Dm.nprocz;
		count++;

		if (count%50 == 0 && Dm.rank==0 )
			printf("Time=%i, Max variation=%f, Global variation=%f \n",count,GlobalMax,GlobalVar);

		if (fabs(GlobalMax) < 1e-5){
			if (Dm.rank==0) printf("Exiting with max tolerance of 1e-5 \n");
			count=timesteps;
		}
	}
	return GlobalVar;
}



// Reading the domain information file
void read_domain( int rank, int nprocs, MPI_Comm comm, 
    int& nprocx, int& nprocy, int& nprocz, int& nx, int& ny, int& nz,
    int& nspheres, double& Lx, double& Ly, double& Lz )
{
	if (rank==0){
		ifstream domain("Domain.in");
		domain >> nprocx;
		domain >> nprocy;
		domain >> nprocz;
		domain >> nx;
		domain >> ny;
		domain >> nz;
		domain >> nspheres;
		domain >> Lx;
		domain >> Ly;
		domain >> Lz;

	}
	MPI_Barrier(comm);
	// Computational domain
	//.................................................
	MPI_Bcast(&nx,1,MPI_INT,0,comm);
	MPI_Bcast(&ny,1,MPI_INT,0,comm);
	MPI_Bcast(&nz,1,MPI_INT,0,comm);
	MPI_Bcast(&nprocx,1,MPI_INT,0,comm);
	MPI_Bcast(&nprocy,1,MPI_INT,0,comm);
	MPI_Bcast(&nprocz,1,MPI_INT,0,comm);
	MPI_Bcast(&nspheres,1,MPI_INT,0,comm);
	MPI_Bcast(&Lx,1,MPI_DOUBLE,0,comm);
	MPI_Bcast(&Ly,1,MPI_DOUBLE,0,comm);
	MPI_Bcast(&Lz,1,MPI_DOUBLE,0,comm);
	MPI_Barrier(comm);
}


/********************************************************
* Constructor/Destructor                                *
********************************************************/
Domain::Domain(int nx, int ny, int nz, int rnk, int npx, int npy, int npz, 
    double lx, double ly, double lz, int BC):
    Nx(0), Ny(0), Nz(0), iproc(0), jproc(0), nprocx(0), nprocy(0), nprocz(0),
    Lx(0), Ly(0), Lz(0), Volume(0), rank(0), BoundaryCondition(0),
    Group(MPI_GROUP_NULL), Comm(MPI_COMM_NULL),
    rank_x(0), rank_y(0), rank_z(0), rank_X(0), rank_Y(0), rank_Z(0),
    rank_xy(0), rank_XY(0), rank_xY(0), rank_Xy(0),
    rank_xz(0), rank_XZ(0), rank_xZ(0), rank_Xz(0),
    rank_yz(0), rank_YZ(0), rank_yZ(0), rank_Yz(0),
    sendCount_x(0), sendCount_y(0), sendCount_z(0), sendCount_X(0), sendCount_Y(0), sendCount_Z(0),
    sendCount_xy(0), sendCount_yz(0), sendCount_xz(0), sendCount_Xy(0), sendCount_Yz(0), sendCount_xZ(0),
    sendCount_xY(0), sendCount_yZ(0), sendCount_Xz(0), sendCount_XY(0), sendCount_YZ(0), sendCount_XZ(0),
    sendList_x(NULL), sendList_y(NULL), sendList_z(NULL), sendList_X(NULL), sendList_Y(NULL), sendList_Z(NULL),
    sendList_xy(NULL), sendList_yz(NULL), sendList_xz(NULL), sendList_Xy(NULL), sendList_Yz(NULL), sendList_xZ(NULL),
    sendList_xY(NULL), sendList_yZ(NULL), sendList_Xz(NULL), sendList_XY(NULL), sendList_YZ(NULL), sendList_XZ(NULL),
    sendBuf_x(NULL), sendBuf_y(NULL), sendBuf_z(NULL), sendBuf_X(NULL), sendBuf_Y(NULL), sendBuf_Z(NULL),
    sendBuf_xy(NULL), sendBuf_yz(NULL), sendBuf_xz(NULL), sendBuf_Xy(NULL), sendBuf_Yz(NULL), sendBuf_xZ(NULL),
    sendBuf_xY(NULL), sendBuf_yZ(NULL), sendBuf_Xz(NULL), sendBuf_XY(NULL), sendBuf_YZ(NULL), sendBuf_XZ(NULL),
    recvCount_x(0), recvCount_y(0), recvCount_z(0), recvCount_X(0), recvCount_Y(0), recvCount_Z(0),
    recvCount_xy(0), recvCount_yz(0), recvCount_xz(0), recvCount_Xy(0), recvCount_Yz(0), recvCount_xZ(0),
    recvCount_xY(0), recvCount_yZ(0), recvCount_Xz(0), recvCount_XY(0), recvCount_YZ(0), recvCount_XZ(0),
    recvList_x(NULL), recvList_y(NULL), recvList_z(NULL), recvList_X(NULL), recvList_Y(NULL), recvList_Z(NULL),
    recvList_xy(NULL), recvList_yz(NULL), recvList_xz(NULL), recvList_Xy(NULL), recvList_Yz(NULL), recvList_xZ(NULL),
    recvList_xY(NULL), recvList_yZ(NULL), recvList_Xz(NULL), recvList_XY(NULL), recvList_YZ(NULL), recvList_XZ(NULL),
    recvBuf_x(NULL), recvBuf_y(NULL), recvBuf_z(NULL), recvBuf_X(NULL), recvBuf_Y(NULL), recvBuf_Z(NULL),
    recvBuf_xy(NULL), recvBuf_yz(NULL), recvBuf_xz(NULL), recvBuf_Xy(NULL), recvBuf_Yz(NULL), recvBuf_xZ(NULL),
    recvBuf_xY(NULL), recvBuf_yZ(NULL), recvBuf_Xz(NULL), recvBuf_XY(NULL), recvBuf_YZ(NULL), recvBuf_XZ(NULL),
    sendData_x(NULL), sendData_y(NULL), sendData_z(NULL), sendData_X(NULL), sendData_Y(NULL), sendData_Z(NULL),
    sendData_xy(NULL), sendData_yz(NULL), sendData_xz(NULL), sendData_Xy(NULL), sendData_Yz(NULL), sendData_xZ(NULL),
    sendData_xY(NULL), sendData_yZ(NULL), sendData_Xz(NULL), sendData_XY(NULL), sendData_YZ(NULL), sendData_XZ(NULL),
    recvData_x(NULL), recvData_y(NULL), recvData_z(NULL), recvData_X(NULL), recvData_Y(NULL), recvData_Z(NULL),
    recvData_xy(NULL), recvData_yz(NULL), recvData_xz(NULL), recvData_Xy(NULL), recvData_Yz(NULL), recvData_xZ(NULL),
    recvData_xY(NULL), recvData_yZ(NULL), recvData_Xz(NULL), recvData_XY(NULL), recvData_YZ(NULL), recvData_XZ(NULL),
    id(NULL)
{
    Volume = nx*ny*nx*npx*npy*npz*1.0;
    Nx = nx+2; Ny = ny+2; Nz = nz+2; 
    Lx = lx, Ly = ly, Lz = lz;
    rank = rnk;
    nprocx=npx; nprocy=npy; nprocz=npz;
    N = Nx*Ny*Nz;
    id = new char[N];
    memset(id,0,N);
    BlobLabel.resize(Nx,Ny,Nz);
    BlobGraph.resize(18,MAX_BLOB_COUNT,MAX_BLOB_COUNT);
    BoundaryCondition = BC;
    rank_info=RankInfoStruct(rank,nprocx,nprocy,nprocz);
}
Domain::~Domain()
{
    // Free sendList
    delete [] sendList_x;   delete [] sendList_y;   delete [] sendList_z;
    delete [] sendList_X;   delete [] sendList_Y;   delete [] sendList_Z;
    delete [] sendList_xy;  delete [] sendList_yz;  delete [] sendList_xz;
    delete [] sendList_Xy;  delete [] sendList_Yz;  delete [] sendList_xZ;
    delete [] sendList_xY;  delete [] sendList_yZ;  delete [] sendList_Xz;
    delete [] sendList_XY;  delete [] sendList_YZ;  delete [] sendList_XZ;
    // Free sendBuf
    delete [] sendBuf_x;    delete [] sendBuf_y;    delete [] sendBuf_z;
    delete [] sendBuf_X;    delete [] sendBuf_Y;    delete [] sendBuf_Z;
    delete [] sendBuf_xy;   delete [] sendBuf_yz;   delete [] sendBuf_xz;
    delete [] sendBuf_Xy;   delete [] sendBuf_Yz;   delete [] sendBuf_xZ;
    delete [] sendBuf_xY;   delete [] sendBuf_yZ;   delete [] sendBuf_Xz;
    delete [] sendBuf_XY;   delete [] sendBuf_YZ;   delete [] sendBuf_XZ;
    // Free recvList
    delete [] recvList_x;   delete [] recvList_y;   delete [] recvList_z;
    delete [] recvList_X;   delete [] recvList_Y;   delete [] recvList_Z;
    delete [] recvList_xy;  delete [] recvList_yz;  delete [] recvList_xz;
    delete [] recvList_Xy;  delete [] recvList_Yz;  delete [] recvList_xZ;
    delete [] recvList_xY;  delete [] recvList_yZ;  delete [] recvList_Xz;
    delete [] recvList_XY;  delete [] recvList_YZ;  delete [] recvList_XZ;
    // Free recvBuf
    delete [] recvBuf_x;    delete [] recvBuf_y;    delete [] recvBuf_z;
    delete [] recvBuf_X;    delete [] recvBuf_Y;    delete [] recvBuf_Z;
    delete [] recvBuf_xy;   delete [] recvBuf_yz;   delete [] recvBuf_xz;
    delete [] recvBuf_Xy;   delete [] recvBuf_Yz;   delete [] recvBuf_xZ;
    delete [] recvBuf_xY;   delete [] recvBuf_yZ;   delete [] recvBuf_Xz;
    delete [] recvBuf_XY;   delete [] recvBuf_YZ;   delete [] recvBuf_XZ;
    // Free sendData
    delete [] sendData_x;   delete [] sendData_y;   delete [] sendData_z;
    delete [] sendData_X;   delete [] sendData_Y;   delete [] sendData_Z;
    delete [] sendData_xy;  delete [] sendData_xY;  delete [] sendData_Xy;
    delete [] sendData_XY;  delete [] sendData_xz;  delete [] sendData_xZ;
    delete [] sendData_Xz;  delete [] sendData_XZ;  delete [] sendData_yz;
    delete [] sendData_yZ;  delete [] sendData_Yz;  delete [] sendData_YZ;
    // Free recvData
    delete [] recvData_x;   delete [] recvData_y;   delete [] recvData_z;
    delete [] recvData_X;   delete [] recvData_Y;   delete [] recvData_Z;
    delete [] recvData_xy;  delete [] recvData_xY;  delete [] recvData_Xy;
    delete [] recvData_XY;  delete [] recvData_xz;  delete [] recvData_xZ;
    delete [] recvData_Xz;  delete [] recvData_XZ;  delete [] recvData_yz;
    delete [] recvData_yZ;  delete [] recvData_Yz;  delete [] recvData_YZ;
    // Free id
    delete [] id;
    // Free the communicator
    if ( Group!=MPI_GROUP_NULL ) {
        MPI_Group_free(&Group);
        MPI_Comm_free(&Comm);
    }
}


    
int Domain::VoxelConnection(int n)
{
    const int d[26][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1},
        {1,1,0},{1,-1,0},{-1,1,0},{-1,-1,0},{1,0,1},{-1,0,1},
        {1,0,-1},{-1,0,-1},{0,1,1},{0,-1,1},{0,1,-1},{0,-1,-1},
        {1,1,1},{1,1,-1},{1,-1,1},{1,-1,-1},{-1,1,1},{-1,1,-1},
        {-1,-1,1},{-1,-1,-1}};   // directions to neighbors
    
    int returnVal = -1;
    int x,y,z;
    // Get the 3-D indices
    x = n%Nx;
    y = (n/Nx)%Ny;
    z = n/(Nx*Ny);
    int nodx,nody,nodz;
    for (int p=0;p<26;p++){
        nodx=x+d[p][0];
        // Get the neighbor and guarantee it is in the domain 
        if (nodx < 0 ){ nodx = 0; }        
        if (nodx > Nx-1 ){ nodx = Nx-1; }
        nody=y+d[p][1];
        if (nody < 0 ){ nody = 0; }        
        if (nody > Ny-1 ){ nody = Ny-1; }
        nodz=z+d[p][2];
        if (nodz < 0 ){ nodz = 0; }        
        if (nodz > Nz-1 ){ nodz = Nz-1; }
        
        if (BlobLabel(nodx,nody,nodz) > returnVal )    returnVal = BlobLabel(nodx,nody,nodz);
    }
    return returnVal;
}
    
void Domain::getBlobConnections(int * List, int count, int neighbor, int direction){

    int idx,n,localValue,neighborValue;
    int x,y,z;
    for (idx=0; idx<count; idx++){
        n = List[idx];
        // Get the 3-D indices
        x = n%Nx;
        y = (n/Nx)%Ny;
        z = n/(Nx*Ny);
        neighborValue = BlobLabel(x,y,z);
        if (neighborValue > -1){
            localValue = VoxelConnection(n);
            printf("Blob (%i,%i) connects to neighbor blob (%i,%i)", localValue, rank, neighborValue, neighbor);
            BlobGraph(direction,localValue,neighbor) = 1; // Set the BlobGraph to TRUE for this pair        
        }
    }
}


void Domain::InitializeRanks()
{
    // map the rank to the block index
    iproc = rank%nprocx;
    jproc = (rank/nprocx)%nprocy;
    kproc = rank/(nprocx*nprocy);

    // set up the neighbor ranks
    int i = iproc;
    int j = jproc;
    int k = kproc;
    rank_X = getRankForBlock(i+1,j,k);
    rank_x = getRankForBlock(i-1,j,k);
    rank_Y = getRankForBlock(i,j+1,k);
    rank_y = getRankForBlock(i,j-1,k);
    rank_Z = getRankForBlock(i,j,k+1);
    rank_z = getRankForBlock(i,j,k-1);
    rank_XY = getRankForBlock(i+1,j+1,k);
    rank_xy = getRankForBlock(i-1,j-1,k);
    rank_Xy = getRankForBlock(i+1,j-1,k);
    rank_xY = getRankForBlock(i-1,j+1,k);
    rank_XZ = getRankForBlock(i+1,j,k+1);
    rank_xz = getRankForBlock(i-1,j,k-1);
    rank_Xz = getRankForBlock(i+1,j,k-1);
    rank_xZ = getRankForBlock(i-1,j,k+1);
    rank_YZ = getRankForBlock(i,j+1,k+1);
    rank_yz = getRankForBlock(i,j-1,k-1);
    rank_Yz = getRankForBlock(i,j+1,k-1);
    rank_yZ = getRankForBlock(i,j-1,k+1);
}


void Domain::CommInit(MPI_Comm Communicator){
    int i,j,k,n;
    int sendtag = 21;
    int recvtag = 21;

    //......................................................................................
    //Get the ranks of each process and it's neighbors
    // map the rank to the block index
    iproc = rank%nprocx;
    jproc = (rank/nprocx)%nprocy;
    kproc = rank/(nprocx*nprocy);
    // set up the neighbor ranks
    i = iproc;
    j = jproc;
    k = kproc;
    rank_X = getRankForBlock(i+1,j,k);
    rank_x = getRankForBlock(i-1,j,k);
    rank_Y = getRankForBlock(i,j+1,k);
    rank_y = getRankForBlock(i,j-1,k);
    rank_Z = getRankForBlock(i,j,k+1);
    rank_z = getRankForBlock(i,j,k-1);
    rank_XY = getRankForBlock(i+1,j+1,k);
    rank_xy = getRankForBlock(i-1,j-1,k);
    rank_Xy = getRankForBlock(i+1,j-1,k);
    rank_xY = getRankForBlock(i-1,j+1,k);
    rank_XZ = getRankForBlock(i+1,j,k+1);
    rank_xz = getRankForBlock(i-1,j,k-1);
    rank_Xz = getRankForBlock(i+1,j,k-1);
    rank_xZ = getRankForBlock(i-1,j,k+1);
    rank_YZ = getRankForBlock(i,j+1,k+1);
    rank_yz = getRankForBlock(i,j-1,k-1);
    rank_Yz = getRankForBlock(i,j+1,k-1);
    rank_yZ = getRankForBlock(i,j-1,k+1);
    //......................................................................................

    MPI_Comm_group(Communicator,&Group);
    MPI_Comm_create(Communicator,Group,&Comm);

    //......................................................................................
    MPI_Request req1[18], req2[18];
    MPI_Status stat1[18],stat2[18];
    //......................................................................................
    sendCount_x = sendCount_y = sendCount_z = sendCount_X = sendCount_Y = sendCount_Z = 0;
    sendCount_xy = sendCount_yz = sendCount_xz = sendCount_Xy = sendCount_Yz = sendCount_xZ = 0;
    sendCount_xY = sendCount_yZ = sendCount_Xz = sendCount_XY = sendCount_YZ = sendCount_XZ = 0;
    //......................................................................................
    for (k=1; k<Nz-1; k++){
        for (j=1; j<Ny-1; j++){
            for (i=1; i<Nx-1; i++){
                // Check the phase ID
                if (id[k*Nx*Ny+j*Nx+i] != 0){
                    // Counts for the six faces
                    if (i==1)    sendCount_x++;
                    if (j==1)    sendCount_y++;
                    if (k==1)    sendCount_z++;
                    if (i==Nx-2)    sendCount_X++;
                    if (j==Ny-2)    sendCount_Y++;
                    if (k==Nz-2)    sendCount_Z++;
                    // Counts for the twelve edges
                    if (i==1 && j==1)    sendCount_xy++;
                    if (i==1 && j==Ny-2)    sendCount_xY++;
                    if (i==Nx-2 && j==1)    sendCount_Xy++;
                    if (i==Nx-2 && j==Ny-2)    sendCount_XY++;

                    if (i==1 && k==1)    sendCount_xz++;
                    if (i==1 && k==Nz-2)    sendCount_xZ++;
                    if (i==Nx-2 && k==1)    sendCount_Xz++;
                    if (i==Nx-2 && k==Nz-2)    sendCount_XZ++;

                    if (j==1 && k==1)    sendCount_yz++;
                    if (j==1 && k==Nz-2)    sendCount_yZ++;
                    if (j==Ny-2 && k==1)    sendCount_Yz++;
                    if (j==Ny-2 && k==Nz-2)    sendCount_YZ++;
                }
            }
        }
    }
    // allocate send lists
    sendList_x = new int [sendCount_x];
    sendList_y = new int [sendCount_y];
    sendList_z = new int [sendCount_z];
    sendList_X = new int [sendCount_X];
    sendList_Y = new int [sendCount_Y];
    sendList_Z = new int [sendCount_Z];
    sendList_xy = new int [sendCount_xy];
    sendList_yz = new int [sendCount_yz];
    sendList_xz = new int [sendCount_xz];
    sendList_Xy = new int [sendCount_Xy];
    sendList_Yz = new int [sendCount_Yz];
    sendList_xZ = new int [sendCount_xZ];
    sendList_xY = new int [sendCount_xY];
    sendList_yZ = new int [sendCount_yZ];
    sendList_Xz = new int [sendCount_Xz];
    sendList_XY = new int [sendCount_XY];
    sendList_YZ = new int [sendCount_YZ];
    sendList_XZ = new int [sendCount_XZ];
    // Populate the send list
    sendCount_x = sendCount_y = sendCount_z = sendCount_X = sendCount_Y = sendCount_Z = 0;
    sendCount_xy = sendCount_yz = sendCount_xz = sendCount_Xy = sendCount_Yz = sendCount_xZ = 0;
    sendCount_xY = sendCount_yZ = sendCount_Xz = sendCount_XY = sendCount_YZ = sendCount_XZ = 0;
    for (k=1; k<Nz-1; k++){
        for (j=1; j<Ny-1; j++){
            for (i=1; i<Nx-1; i++){
                // Local value to send
                n = k*Nx*Ny+j*Nx+i;
                if (id[n] != 0){
                    // Counts for the six faces
                    if (i==1)        sendList_x[sendCount_x++]=n;
                    if (j==1)        sendList_y[sendCount_y++]=n;
                    if (k==1)        sendList_z[sendCount_z++]=n;
                    if (i==Nx-2)    sendList_X[sendCount_X++]=n;
                    if (j==Ny-2)    sendList_Y[sendCount_Y++]=n;
                    if (k==Nz-2)    sendList_Z[sendCount_Z++]=n;
                    // Counts for the twelve edges
                    if (i==1 && j==1)        sendList_xy[sendCount_xy++]=n;
                    if (i==1 && j==Ny-2)    sendList_xY[sendCount_xY++]=n;
                    if (i==Nx-2 && j==1)    sendList_Xy[sendCount_Xy++]=n;
                    if (i==Nx-2 && j==Ny-2)    sendList_XY[sendCount_XY++]=n;

                    if (i==1 && k==1)        sendList_xz[sendCount_xz++]=n;
                    if (i==1 && k==Nz-2)    sendList_xZ[sendCount_xZ++]=n;
                    if (i==Nx-2 && k==1)    sendList_Xz[sendCount_Xz++]=n;
                    if (i==Nx-2 && k==Nz-2)    sendList_XZ[sendCount_XZ++]=n;

                    if (j==1 && k==1)        sendList_yz[sendCount_yz++]=n;
                    if (j==1 && k==Nz-2)    sendList_yZ[sendCount_yZ++]=n;
                    if (j==Ny-2 && k==1)    sendList_Yz[sendCount_Yz++]=n;
                    if (j==Ny-2 && k==Nz-2)    sendList_YZ[sendCount_YZ++]=n;
                }
            }
        }
    }
    // allocate send buffers
    sendBuf_x = new int [sendCount_x];
    sendBuf_y = new int [sendCount_y];
    sendBuf_z = new int [sendCount_z];
    sendBuf_X = new int [sendCount_X];
    sendBuf_Y = new int [sendCount_Y];
    sendBuf_Z = new int [sendCount_Z];
    sendBuf_xy = new int [sendCount_xy];
    sendBuf_yz = new int [sendCount_yz];
    sendBuf_xz = new int [sendCount_xz];
    sendBuf_Xy = new int [sendCount_Xy];
    sendBuf_Yz = new int [sendCount_Yz];
    sendBuf_xZ = new int [sendCount_xZ];
    sendBuf_xY = new int [sendCount_xY];
    sendBuf_yZ = new int [sendCount_yZ];
    sendBuf_Xz = new int [sendCount_Xz];
    sendBuf_XY = new int [sendCount_XY];
    sendBuf_YZ = new int [sendCount_YZ];
    sendBuf_XZ = new int [sendCount_XZ];
    //......................................................................................
    MPI_Isend(&sendCount_x, 1,MPI_INT,rank_x,sendtag+0,Communicator,&req1[0]);
    MPI_Irecv(&recvCount_X, 1,MPI_INT,rank_X,recvtag+0,Communicator,&req2[0]);
    MPI_Isend(&sendCount_X, 1,MPI_INT,rank_X,sendtag+1,Communicator,&req1[1]);
    MPI_Irecv(&recvCount_x, 1,MPI_INT,rank_x,recvtag+1,Communicator,&req2[1]);
    MPI_Isend(&sendCount_y, 1,MPI_INT,rank_y,sendtag+2,Communicator,&req1[2]);
    MPI_Irecv(&recvCount_Y, 1,MPI_INT,rank_Y,recvtag+2,Communicator,&req2[2]);
    MPI_Isend(&sendCount_Y, 1,MPI_INT,rank_Y,sendtag+3,Communicator,&req1[3]);
    MPI_Irecv(&recvCount_y, 1,MPI_INT,rank_y,recvtag+3,Communicator,&req2[3]);
    MPI_Isend(&sendCount_z, 1,MPI_INT,rank_z,sendtag+4,Communicator,&req1[4]);
    MPI_Irecv(&recvCount_Z, 1,MPI_INT,rank_Z,recvtag+4,Communicator,&req2[4]);
    MPI_Isend(&sendCount_Z, 1,MPI_INT,rank_Z,sendtag+5,Communicator,&req1[5]);
    MPI_Irecv(&recvCount_z, 1,MPI_INT,rank_z,recvtag+5,Communicator,&req2[5]);
    MPI_Isend(&sendCount_xy, 1,MPI_INT,rank_xy,sendtag+6,Communicator,&req1[6]);
    MPI_Irecv(&recvCount_XY, 1,MPI_INT,rank_XY,recvtag+6,Communicator,&req2[6]);
    MPI_Isend(&sendCount_XY, 1,MPI_INT,rank_XY,sendtag+7,Communicator,&req1[7]);
    MPI_Irecv(&recvCount_xy, 1,MPI_INT,rank_xy,recvtag+7,Communicator,&req2[7]);
    MPI_Isend(&sendCount_Xy, 1,MPI_INT,rank_Xy,sendtag+8,Communicator,&req1[8]);
    MPI_Irecv(&recvCount_xY, 1,MPI_INT,rank_xY,recvtag+8,Communicator,&req2[8]);
    MPI_Isend(&sendCount_xY, 1,MPI_INT,rank_xY,sendtag+9,Communicator,&req1[9]);
    MPI_Irecv(&recvCount_Xy, 1,MPI_INT,rank_Xy,recvtag+9,Communicator,&req2[9]);
    MPI_Isend(&sendCount_xz, 1,MPI_INT,rank_xz,sendtag+10,Communicator,&req1[10]);
    MPI_Irecv(&recvCount_XZ, 1,MPI_INT,rank_XZ,recvtag+10,Communicator,&req2[10]);
    MPI_Isend(&sendCount_XZ, 1,MPI_INT,rank_XZ,sendtag+11,Communicator,&req1[11]);
    MPI_Irecv(&recvCount_xz, 1,MPI_INT,rank_xz,recvtag+11,Communicator,&req2[11]);
    MPI_Isend(&sendCount_Xz, 1,MPI_INT,rank_Xz,sendtag+12,Communicator,&req1[12]);
    MPI_Irecv(&recvCount_xZ, 1,MPI_INT,rank_xZ,recvtag+12,Communicator,&req2[12]);
    MPI_Isend(&sendCount_xZ, 1,MPI_INT,rank_xZ,sendtag+13,Communicator,&req1[13]);
    MPI_Irecv(&recvCount_Xz, 1,MPI_INT,rank_Xz,recvtag+13,Communicator,&req2[13]);
    MPI_Isend(&sendCount_yz, 1,MPI_INT,rank_yz,sendtag+14,Communicator,&req1[14]);
    MPI_Irecv(&recvCount_YZ, 1,MPI_INT,rank_YZ,recvtag+14,Communicator,&req2[14]);
    MPI_Isend(&sendCount_YZ, 1,MPI_INT,rank_YZ,sendtag+15,Communicator,&req1[15]);
    MPI_Irecv(&recvCount_yz, 1,MPI_INT,rank_yz,recvtag+15,Communicator,&req2[15]);
    MPI_Isend(&sendCount_Yz, 1,MPI_INT,rank_Yz,sendtag+16,Communicator,&req1[16]);
    MPI_Irecv(&recvCount_yZ, 1,MPI_INT,rank_yZ,recvtag+16,Communicator,&req2[16]);
    MPI_Isend(&sendCount_yZ, 1,MPI_INT,rank_yZ,sendtag+17,Communicator,&req1[17]);
    MPI_Irecv(&recvCount_Yz, 1,MPI_INT,rank_Yz,recvtag+17,Communicator,&req2[17]);
    MPI_Waitall(18,req1,stat1);
    MPI_Waitall(18,req2,stat2);
    MPI_Barrier(Communicator);
    //......................................................................................
    // recv buffers
    recvList_x = new int [recvCount_x];
    recvList_y = new int [recvCount_y];
    recvList_z = new int [recvCount_z];
    recvList_X = new int [recvCount_X];
    recvList_Y = new int [recvCount_Y];
    recvList_Z = new int [recvCount_Z];
    recvList_xy = new int [recvCount_xy];
    recvList_yz = new int [recvCount_yz];
    recvList_xz = new int [recvCount_xz];
    recvList_Xy = new int [recvCount_Xy];
    recvList_Yz = new int [recvCount_Yz];
    recvList_xZ = new int [recvCount_xZ];
    recvList_xY = new int [recvCount_xY];
    recvList_yZ = new int [recvCount_yZ];
    recvList_Xz = new int [recvCount_Xz];
    recvList_XY = new int [recvCount_XY];
    recvList_YZ = new int [recvCount_YZ];
    recvList_XZ = new int [recvCount_XZ];
    //......................................................................................
    MPI_Isend(sendList_x, sendCount_x,MPI_INT,rank_x,sendtag,Communicator,&req1[0]);
    MPI_Irecv(recvList_X, recvCount_X,MPI_INT,rank_X,recvtag,Communicator,&req2[0]);
    MPI_Isend(sendList_X, sendCount_X,MPI_INT,rank_X,sendtag,Communicator,&req1[1]);
    MPI_Irecv(recvList_x, recvCount_x,MPI_INT,rank_x,recvtag,Communicator,&req2[1]);
    MPI_Isend(sendList_y, sendCount_y,MPI_INT,rank_y,sendtag,Communicator,&req1[2]);
    MPI_Irecv(recvList_Y, recvCount_Y,MPI_INT,rank_Y,recvtag,Communicator,&req2[2]);
    MPI_Isend(sendList_Y, sendCount_Y,MPI_INT,rank_Y,sendtag,Communicator,&req1[3]);
    MPI_Irecv(recvList_y, recvCount_y,MPI_INT,rank_y,recvtag,Communicator,&req2[3]);
    MPI_Isend(sendList_z, sendCount_z,MPI_INT,rank_z,sendtag,Communicator,&req1[4]);
    MPI_Irecv(recvList_Z, recvCount_Z,MPI_INT,rank_Z,recvtag,Communicator,&req2[4]);
    MPI_Isend(sendList_Z, sendCount_Z,MPI_INT,rank_Z,sendtag,Communicator,&req1[5]);
    MPI_Irecv(recvList_z, recvCount_z,MPI_INT,rank_z,recvtag,Communicator,&req2[5]);
    MPI_Isend(sendList_xy, sendCount_xy,MPI_INT,rank_xy,sendtag,Communicator,&req1[6]);
    MPI_Irecv(recvList_XY, recvCount_XY,MPI_INT,rank_XY,recvtag,Communicator,&req2[6]);
    MPI_Isend(sendList_XY, sendCount_XY,MPI_INT,rank_XY,sendtag,Communicator,&req1[7]);
    MPI_Irecv(recvList_xy, recvCount_xy,MPI_INT,rank_xy,recvtag,Communicator,&req2[7]);
    MPI_Isend(sendList_Xy, sendCount_Xy,MPI_INT,rank_Xy,sendtag,Communicator,&req1[8]);
    MPI_Irecv(recvList_xY, recvCount_xY,MPI_INT,rank_xY,recvtag,Communicator,&req2[8]);
    MPI_Isend(sendList_xY, sendCount_xY,MPI_INT,rank_xY,sendtag,Communicator,&req1[9]);
    MPI_Irecv(recvList_Xy, recvCount_Xy,MPI_INT,rank_Xy,recvtag,Communicator,&req2[9]);
    MPI_Isend(sendList_xz, sendCount_xz,MPI_INT,rank_xz,sendtag,Communicator,&req1[10]);
    MPI_Irecv(recvList_XZ, recvCount_XZ,MPI_INT,rank_XZ,recvtag,Communicator,&req2[10]);
    MPI_Isend(sendList_XZ, sendCount_XZ,MPI_INT,rank_XZ,sendtag,Communicator,&req1[11]);
    MPI_Irecv(recvList_xz, recvCount_xz,MPI_INT,rank_xz,recvtag,Communicator,&req2[11]);
    MPI_Isend(sendList_Xz, sendCount_Xz,MPI_INT,rank_Xz,sendtag,Communicator,&req1[12]);
    MPI_Irecv(recvList_xZ, recvCount_xZ,MPI_INT,rank_xZ,recvtag,Communicator,&req2[12]);
    MPI_Isend(sendList_xZ, sendCount_xZ,MPI_INT,rank_xZ,sendtag,Communicator,&req1[13]);
    MPI_Irecv(recvList_Xz, recvCount_Xz,MPI_INT,rank_Xz,recvtag,Communicator,&req2[13]);
    MPI_Isend(sendList_yz, sendCount_yz,MPI_INT,rank_yz,sendtag,Communicator,&req1[14]);
    MPI_Irecv(recvList_YZ, recvCount_YZ,MPI_INT,rank_YZ,recvtag,Communicator,&req2[14]);
    MPI_Isend(sendList_YZ, sendCount_YZ,MPI_INT,rank_YZ,sendtag,Communicator,&req1[15]);
    MPI_Irecv(recvList_yz, recvCount_yz,MPI_INT,rank_yz,recvtag,Communicator,&req2[15]);
    MPI_Isend(sendList_Yz, sendCount_Yz,MPI_INT,rank_Yz,sendtag,Communicator,&req1[16]);
    MPI_Irecv(recvList_yZ, recvCount_yZ,MPI_INT,rank_yZ,recvtag,Communicator,&req2[16]);
    MPI_Isend(sendList_yZ, sendCount_yZ,MPI_INT,rank_yZ,sendtag,Communicator,&req1[17]);
    MPI_Irecv(recvList_Yz, recvCount_Yz,MPI_INT,rank_Yz,recvtag,Communicator,&req2[17]);
    MPI_Waitall(18,req1,stat1);
    MPI_Waitall(18,req2,stat2);
    //......................................................................................
    for (int idx=0; idx<recvCount_x; idx++)    recvList_x[idx] -= (Nx-2);
    for (int idx=0; idx<recvCount_X; idx++)    recvList_X[idx] += (Nx-2);
    for (int idx=0; idx<recvCount_y; idx++)    recvList_y[idx] -= (Ny-2)*Nx;
    for (int idx=0; idx<recvCount_Y; idx++)    recvList_Y[idx] += (Ny-2)*Nx;
    for (int idx=0; idx<recvCount_z; idx++)    recvList_z[idx] -= (Nz-2)*Nx*Ny;
    for (int idx=0; idx<recvCount_Z; idx++)    recvList_Z[idx] += (Nz-2)*Nx*Ny;    
    for (int idx=0; idx<recvCount_xy; idx++)    recvList_xy[idx] -= (Nx-2)+(Ny-2)*Nx;
    for (int idx=0; idx<recvCount_XY; idx++)    recvList_XY[idx] += (Nx-2)+(Ny-2)*Nx;
    for (int idx=0; idx<recvCount_xY; idx++)    recvList_xY[idx] -= (Nx-2)-(Ny-2)*Nx;
    for (int idx=0; idx<recvCount_Xy; idx++)    recvList_Xy[idx] += (Nx-2)-(Ny-2)*Nx;
    for (int idx=0; idx<recvCount_xz; idx++)    recvList_xz[idx] -= (Nx-2)+(Nz-2)*Nx*Ny;
    for (int idx=0; idx<recvCount_XZ; idx++)    recvList_XZ[idx] += (Nx-2)+(Nz-2)*Nx*Ny;
    for (int idx=0; idx<recvCount_xZ; idx++)    recvList_xZ[idx] -= (Nx-2)-(Nz-2)*Nx*Ny;
    for (int idx=0; idx<recvCount_Xz; idx++)    recvList_Xz[idx] += (Nx-2)-(Nz-2)*Nx*Ny;
    for (int idx=0; idx<recvCount_yz; idx++)    recvList_yz[idx] -= (Ny-2)*Nx + (Nz-2)*Nx*Ny;
    for (int idx=0; idx<recvCount_YZ; idx++)    recvList_YZ[idx] += (Ny-2)*Nx + (Nz-2)*Nx*Ny;
    for (int idx=0; idx<recvCount_yZ; idx++)    recvList_yZ[idx] -= (Ny-2)*Nx - (Nz-2)*Nx*Ny;
    for (int idx=0; idx<recvCount_Yz; idx++)    recvList_Yz[idx] += (Ny-2)*Nx - (Nz-2)*Nx*Ny;
    //......................................................................................
    // allocate recv buffers
    recvBuf_x = new int [recvCount_x];
    recvBuf_y = new int [recvCount_y];
    recvBuf_z = new int [recvCount_z];
    recvBuf_X = new int [recvCount_X];
    recvBuf_Y = new int [recvCount_Y];
    recvBuf_Z = new int [recvCount_Z];
    recvBuf_xy = new int [recvCount_xy];
    recvBuf_yz = new int [recvCount_yz];
    recvBuf_xz = new int [recvCount_xz];
    recvBuf_Xy = new int [recvCount_Xy];
    recvBuf_Yz = new int [recvCount_Yz];
    recvBuf_xZ = new int [recvCount_xZ];
    recvBuf_xY = new int [recvCount_xY];
    recvBuf_yZ = new int [recvCount_yZ];
    recvBuf_Xz = new int [recvCount_Xz];
    recvBuf_XY = new int [recvCount_XY];
    recvBuf_YZ = new int [recvCount_YZ];
    recvBuf_XZ = new int [recvCount_XZ];
    //......................................................................................
    // send buffers
    sendData_x = new double [sendCount_x];
    sendData_y = new double [sendCount_y];
    sendData_z = new double [sendCount_z];
    sendData_X = new double [sendCount_X];
    sendData_Y = new double [sendCount_Y];
    sendData_Z = new double [sendCount_Z];
    sendData_xy = new double [sendCount_xy];
    sendData_yz = new double [sendCount_yz];
    sendData_xz = new double [sendCount_xz];
    sendData_Xy = new double [sendCount_Xy];
    sendData_Yz = new double [sendCount_Yz];
    sendData_xZ = new double [sendCount_xZ];
    sendData_xY = new double [sendCount_xY];
    sendData_yZ = new double [sendCount_yZ];
    sendData_Xz = new double [sendCount_Xz];
    sendData_XY = new double [sendCount_XY];
    sendData_YZ = new double [sendCount_YZ];
    sendData_XZ = new double [sendCount_XZ];
    //......................................................................................
    // recv buffers
    recvData_x = new double [recvCount_x];
    recvData_y = new double [recvCount_y];
    recvData_z = new double [recvCount_z];
    recvData_X = new double [recvCount_X];
    recvData_Y = new double [recvCount_Y];
    recvData_Z = new double [recvCount_Z];
    recvData_xy = new double [recvCount_xy];
    recvData_yz = new double [recvCount_yz];
    recvData_xz = new double [recvCount_xz];
    recvData_Xy = new double [recvCount_Xy];
    recvData_xZ = new double [recvCount_xZ];
    recvData_xY = new double [recvCount_xY];
    recvData_yZ = new double [recvCount_yZ];
    recvData_Yz = new double [recvCount_Yz];
    recvData_Xz = new double [recvCount_Xz];
    recvData_XY = new double [recvCount_XY];
    recvData_YZ = new double [recvCount_YZ];
    recvData_XZ = new double [recvCount_XZ];
    //......................................................................................

}

void Domain::CommunicateMeshHalo(DoubleArray &Mesh)
{
    int sendtag, recvtag;
    sendtag = recvtag = 7;
    double *MeshData = Mesh.data();
    PackMeshData(sendList_x, sendCount_x ,sendData_x, MeshData);
    PackMeshData(sendList_X, sendCount_X ,sendData_X, MeshData);
    PackMeshData(sendList_y, sendCount_y ,sendData_y, MeshData);
    PackMeshData(sendList_Y, sendCount_Y ,sendData_Y, MeshData);
    PackMeshData(sendList_z, sendCount_z ,sendData_z, MeshData);
    PackMeshData(sendList_Z, sendCount_Z ,sendData_Z, MeshData);
    PackMeshData(sendList_xy, sendCount_xy ,sendData_xy, MeshData);
    PackMeshData(sendList_Xy, sendCount_Xy ,sendData_Xy, MeshData);
    PackMeshData(sendList_xY, sendCount_xY ,sendData_xY, MeshData);
    PackMeshData(sendList_XY, sendCount_XY ,sendData_XY, MeshData);
    PackMeshData(sendList_xz, sendCount_xz ,sendData_xz, MeshData);
    PackMeshData(sendList_Xz, sendCount_Xz ,sendData_Xz, MeshData);
    PackMeshData(sendList_xZ, sendCount_xZ ,sendData_xZ, MeshData);
    PackMeshData(sendList_XZ, sendCount_XZ ,sendData_XZ, MeshData);
    PackMeshData(sendList_yz, sendCount_yz ,sendData_yz, MeshData);
    PackMeshData(sendList_Yz, sendCount_Yz ,sendData_Yz, MeshData);
    PackMeshData(sendList_yZ, sendCount_yZ ,sendData_yZ, MeshData);
    PackMeshData(sendList_YZ, sendCount_YZ ,sendData_YZ, MeshData);
    //......................................................................................
    MPI_Sendrecv(sendData_x,sendCount_x,MPI_DOUBLE,rank_x,sendtag,
            recvData_X,recvCount_X,MPI_DOUBLE,rank_X,recvtag,Comm,MPI_STATUS_IGNORE);
    MPI_Sendrecv(sendData_X,sendCount_X,MPI_DOUBLE,rank_X,sendtag,
            recvData_x,recvCount_x,MPI_DOUBLE,rank_x,recvtag,Comm,MPI_STATUS_IGNORE);
    MPI_Sendrecv(sendData_y,sendCount_y,MPI_DOUBLE,rank_y,sendtag,
            recvData_Y,recvCount_Y,MPI_DOUBLE,rank_Y,recvtag,Comm,MPI_STATUS_IGNORE);
    MPI_Sendrecv(sendData_Y,sendCount_Y,MPI_DOUBLE,rank_Y,sendtag,
            recvData_y,recvCount_y,MPI_DOUBLE,rank_y,recvtag,Comm,MPI_STATUS_IGNORE);
    MPI_Sendrecv(sendData_z,sendCount_z,MPI_DOUBLE,rank_z,sendtag,
            recvData_Z,recvCount_Z,MPI_DOUBLE,rank_Z,recvtag,Comm,MPI_STATUS_IGNORE);
    MPI_Sendrecv(sendData_Z,sendCount_Z,MPI_DOUBLE,rank_Z,sendtag,
            recvData_z,recvCount_z,MPI_DOUBLE,rank_z,recvtag,Comm,MPI_STATUS_IGNORE);
    MPI_Sendrecv(sendData_xy,sendCount_xy,MPI_DOUBLE,rank_xy,sendtag,
            recvData_XY,recvCount_XY,MPI_DOUBLE,rank_XY,recvtag,Comm,MPI_STATUS_IGNORE);
    MPI_Sendrecv(sendData_XY,sendCount_XY,MPI_DOUBLE,rank_XY,sendtag,
            recvData_xy,recvCount_xy,MPI_DOUBLE,rank_xy,recvtag,Comm,MPI_STATUS_IGNORE);
    MPI_Sendrecv(sendData_Xy,sendCount_Xy,MPI_DOUBLE,rank_Xy,sendtag,
            recvData_xY,recvCount_xY,MPI_DOUBLE,rank_xY,recvtag,Comm,MPI_STATUS_IGNORE);
    MPI_Sendrecv(sendData_xY,sendCount_xY,MPI_DOUBLE,rank_xY,sendtag,
            recvData_Xy,recvCount_Xy,MPI_DOUBLE,rank_Xy,recvtag,Comm,MPI_STATUS_IGNORE);
    MPI_Sendrecv(sendData_xz,sendCount_xz,MPI_DOUBLE,rank_xz,sendtag,
            recvData_XZ,recvCount_XZ,MPI_DOUBLE,rank_XZ,recvtag,Comm,MPI_STATUS_IGNORE);
    MPI_Sendrecv(sendData_XZ,sendCount_XZ,MPI_DOUBLE,rank_XZ,sendtag,
            recvData_xz,recvCount_xz,MPI_DOUBLE,rank_xz,recvtag,Comm,MPI_STATUS_IGNORE);
    MPI_Sendrecv(sendData_Xz,sendCount_Xz,MPI_DOUBLE,rank_Xz,sendtag,
            recvData_xZ,recvCount_xZ,MPI_DOUBLE,rank_xZ,recvtag,Comm,MPI_STATUS_IGNORE);
    MPI_Sendrecv(sendData_xZ,sendCount_xZ,MPI_DOUBLE,rank_xZ,sendtag,
            recvData_Xz,recvCount_Xz,MPI_DOUBLE,rank_Xz,recvtag,Comm,MPI_STATUS_IGNORE);
    MPI_Sendrecv(sendData_yz,sendCount_yz,MPI_DOUBLE,rank_yz,sendtag,
            recvData_YZ,recvCount_YZ,MPI_DOUBLE,rank_YZ,recvtag,Comm,MPI_STATUS_IGNORE);
    MPI_Sendrecv(sendData_YZ,sendCount_YZ,MPI_DOUBLE,rank_YZ,sendtag,
            recvData_yz,recvCount_yz,MPI_DOUBLE,rank_yz,recvtag,Comm,MPI_STATUS_IGNORE);
    MPI_Sendrecv(sendData_Yz,sendCount_Yz,MPI_DOUBLE,rank_Yz,sendtag,
            recvData_yZ,recvCount_yZ,MPI_DOUBLE,rank_yZ,recvtag,Comm,MPI_STATUS_IGNORE);
    MPI_Sendrecv(sendData_yZ,sendCount_yZ,MPI_DOUBLE,rank_yZ,sendtag,
            recvData_Yz,recvCount_Yz,MPI_DOUBLE,rank_Yz,recvtag,Comm,MPI_STATUS_IGNORE);
    //........................................................................................
    UnpackMeshData(recvList_x, recvCount_x ,recvData_x, MeshData);
    UnpackMeshData(recvList_X, recvCount_X ,recvData_X, MeshData);
    UnpackMeshData(recvList_y, recvCount_y ,recvData_y, MeshData);
    UnpackMeshData(recvList_Y, recvCount_Y ,recvData_Y, MeshData);
    UnpackMeshData(recvList_z, recvCount_z ,recvData_z, MeshData);
    UnpackMeshData(recvList_Z, recvCount_Z ,recvData_Z, MeshData);
    UnpackMeshData(recvList_xy, recvCount_xy ,recvData_xy, MeshData);
    UnpackMeshData(recvList_Xy, recvCount_Xy ,recvData_Xy, MeshData);
    UnpackMeshData(recvList_xY, recvCount_xY ,recvData_xY, MeshData);
    UnpackMeshData(recvList_XY, recvCount_XY ,recvData_XY, MeshData);
    UnpackMeshData(recvList_xz, recvCount_xz ,recvData_xz, MeshData);
    UnpackMeshData(recvList_Xz, recvCount_Xz ,recvData_Xz, MeshData);
    UnpackMeshData(recvList_xZ, recvCount_xZ ,recvData_xZ, MeshData);
    UnpackMeshData(recvList_XZ, recvCount_XZ ,recvData_XZ, MeshData);
    UnpackMeshData(recvList_yz, recvCount_yz ,recvData_yz, MeshData);
    UnpackMeshData(recvList_Yz, recvCount_Yz ,recvData_Yz, MeshData);
    UnpackMeshData(recvList_yZ, recvCount_yZ ,recvData_yZ, MeshData);
    UnpackMeshData(recvList_YZ, recvCount_YZ ,recvData_YZ, MeshData);
}


void Domain::BlobComm(MPI_Comm Communicator)
{
    //......................................................................................
    int sendtag, recvtag;
    sendtag = recvtag = 51;
    //......................................................................................
    int *BlobLabelData = BlobLabel.data();
    PackBlobData(sendList_x, sendCount_x ,sendBuf_x, BlobLabelData);
    PackBlobData(sendList_X, sendCount_X ,sendBuf_X, BlobLabelData);
    PackBlobData(sendList_y, sendCount_y ,sendBuf_y, BlobLabelData);
    PackBlobData(sendList_Y, sendCount_Y ,sendBuf_Y, BlobLabelData);
    PackBlobData(sendList_z, sendCount_z ,sendBuf_z, BlobLabelData);
    PackBlobData(sendList_Z, sendCount_Z ,sendBuf_Z, BlobLabelData);
    PackBlobData(sendList_xy, sendCount_xy ,sendBuf_xy, BlobLabelData);
    PackBlobData(sendList_Xy, sendCount_Xy ,sendBuf_Xy, BlobLabelData);
    PackBlobData(sendList_xY, sendCount_xY ,sendBuf_xY, BlobLabelData);
    PackBlobData(sendList_XY, sendCount_XY ,sendBuf_XY, BlobLabelData);
    PackBlobData(sendList_xz, sendCount_xz ,sendBuf_xz, BlobLabelData);
    PackBlobData(sendList_Xz, sendCount_Xz ,sendBuf_Xz, BlobLabelData);
    PackBlobData(sendList_xZ, sendCount_xZ ,sendBuf_xZ, BlobLabelData);
    PackBlobData(sendList_XZ, sendCount_XZ ,sendBuf_XZ, BlobLabelData);
    PackBlobData(sendList_yz, sendCount_yz ,sendBuf_yz, BlobLabelData);
    PackBlobData(sendList_Yz, sendCount_Yz ,sendBuf_Yz, BlobLabelData);
    PackBlobData(sendList_yZ, sendCount_yZ ,sendBuf_yZ, BlobLabelData);
    PackBlobData(sendList_YZ, sendCount_YZ ,sendBuf_YZ, BlobLabelData);
    //......................................................................................
    MPI_Sendrecv(sendBuf_x,sendCount_x,MPI_INT,rank_x,sendtag,
            recvBuf_X,recvCount_X,MPI_INT,rank_X,recvtag,Comm,MPI_STATUS_IGNORE);
    MPI_Sendrecv(sendBuf_X,sendCount_X,MPI_INT,rank_X,sendtag,
            recvBuf_x,recvCount_x,MPI_INT,rank_x,recvtag,Comm,MPI_STATUS_IGNORE);
    MPI_Sendrecv(sendBuf_y,sendCount_y,MPI_INT,rank_y,sendtag,
            recvBuf_Y,recvCount_Y,MPI_INT,rank_Y,recvtag,Comm,MPI_STATUS_IGNORE);
    MPI_Sendrecv(sendBuf_Y,sendCount_Y,MPI_INT,rank_Y,sendtag,
            recvBuf_y,recvCount_y,MPI_INT,rank_y,recvtag,Comm,MPI_STATUS_IGNORE);
    MPI_Sendrecv(sendBuf_z,sendCount_z,MPI_INT,rank_z,sendtag,
            recvBuf_Z,recvCount_Z,MPI_INT,rank_Z,recvtag,Comm,MPI_STATUS_IGNORE);
    MPI_Sendrecv(sendBuf_Z,sendCount_Z,MPI_INT,rank_Z,sendtag,
            recvBuf_z,recvCount_z,MPI_INT,rank_z,recvtag,Comm,MPI_STATUS_IGNORE);
    MPI_Sendrecv(sendBuf_xy,sendCount_xy,MPI_INT,rank_xy,sendtag,
            recvBuf_XY,recvCount_XY,MPI_INT,rank_XY,recvtag,Comm,MPI_STATUS_IGNORE);
    MPI_Sendrecv(sendBuf_XY,sendCount_XY,MPI_INT,rank_XY,sendtag,
            recvBuf_xy,recvCount_xy,MPI_INT,rank_xy,recvtag,Comm,MPI_STATUS_IGNORE);
    MPI_Sendrecv(sendBuf_Xy,sendCount_Xy,MPI_INT,rank_Xy,sendtag,
            recvBuf_xY,recvCount_xY,MPI_INT,rank_xY,recvtag,Comm,MPI_STATUS_IGNORE);
    MPI_Sendrecv(sendBuf_xY,sendCount_xY,MPI_INT,rank_xY,sendtag,
            recvBuf_Xy,recvCount_Xy,MPI_INT,rank_Xy,recvtag,Comm,MPI_STATUS_IGNORE);
    MPI_Sendrecv(sendBuf_xz,sendCount_xz,MPI_INT,rank_xz,sendtag,
            recvBuf_XZ,recvCount_XZ,MPI_INT,rank_XZ,recvtag,Comm,MPI_STATUS_IGNORE);
    MPI_Sendrecv(sendBuf_XZ,sendCount_XZ,MPI_INT,rank_XZ,sendtag,
            recvBuf_xz,recvCount_xz,MPI_INT,rank_xz,recvtag,Comm,MPI_STATUS_IGNORE);
    MPI_Sendrecv(sendBuf_Xz,sendCount_Xz,MPI_INT,rank_Xz,sendtag,
            recvBuf_xZ,recvCount_xZ,MPI_INT,rank_xZ,recvtag,Comm,MPI_STATUS_IGNORE);
    MPI_Sendrecv(sendBuf_xZ,sendCount_xZ,MPI_INT,rank_xZ,sendtag,
            recvBuf_Xz,recvCount_Xz,MPI_INT,rank_Xz,recvtag,Comm,MPI_STATUS_IGNORE);
    MPI_Sendrecv(sendBuf_yz,sendCount_yz,MPI_INT,rank_yz,sendtag,
            recvBuf_YZ,recvCount_YZ,MPI_INT,rank_YZ,recvtag,Comm,MPI_STATUS_IGNORE);
    MPI_Sendrecv(sendBuf_YZ,sendCount_YZ,MPI_INT,rank_YZ,sendtag,
            recvBuf_yz,recvCount_yz,MPI_INT,rank_yz,recvtag,Comm,MPI_STATUS_IGNORE);
    MPI_Sendrecv(sendBuf_Yz,sendCount_Yz,MPI_INT,rank_Yz,sendtag,
            recvBuf_yZ,recvCount_yZ,MPI_INT,rank_yZ,recvtag,Comm,MPI_STATUS_IGNORE);
    MPI_Sendrecv(sendBuf_yZ,sendCount_yZ,MPI_INT,rank_yZ,sendtag,
            recvBuf_Yz,recvCount_Yz,MPI_INT,rank_Yz,recvtag,Comm,MPI_STATUS_IGNORE);
    //........................................................................................
    UnpackBlobData(recvList_x, recvCount_x ,recvBuf_x, BlobLabelData);
    UnpackBlobData(recvList_X, recvCount_X ,recvBuf_X, BlobLabelData);
    UnpackBlobData(recvList_y, recvCount_y ,recvBuf_y, BlobLabelData);
    UnpackBlobData(recvList_Y, recvCount_Y ,recvBuf_Y, BlobLabelData);
    UnpackBlobData(recvList_z, recvCount_z ,recvBuf_z, BlobLabelData);
    UnpackBlobData(recvList_Z, recvCount_Z ,recvBuf_Z, BlobLabelData);
    UnpackBlobData(recvList_xy, recvCount_xy ,recvBuf_xy, BlobLabelData);
    UnpackBlobData(recvList_Xy, recvCount_Xy ,recvBuf_Xy, BlobLabelData);
    UnpackBlobData(recvList_xY, recvCount_xY ,recvBuf_xY, BlobLabelData);
    UnpackBlobData(recvList_XY, recvCount_XY ,recvBuf_XY, BlobLabelData);
    UnpackBlobData(recvList_xz, recvCount_xz ,recvBuf_xz, BlobLabelData);
    UnpackBlobData(recvList_Xz, recvCount_Xz ,recvBuf_Xz, BlobLabelData);
    UnpackBlobData(recvList_xZ, recvCount_xZ ,recvBuf_xZ, BlobLabelData);
    UnpackBlobData(recvList_XZ, recvCount_XZ ,recvBuf_XZ, BlobLabelData);
    UnpackBlobData(recvList_yz, recvCount_yz ,recvBuf_yz, BlobLabelData);
    UnpackBlobData(recvList_Yz, recvCount_Yz ,recvBuf_Yz, BlobLabelData);
    UnpackBlobData(recvList_yZ, recvCount_yZ ,recvBuf_yZ, BlobLabelData);
    UnpackBlobData(recvList_YZ, recvCount_YZ ,recvBuf_YZ, BlobLabelData);
    //......................................................................................
}


