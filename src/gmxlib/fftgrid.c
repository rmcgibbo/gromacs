#include <stdio.h>
#include "typedefs.h"
#include "futil.h"
#include "fftgrid.h"
#include "smalloc.h"
#include "futil.h"


t_fftgrid *mk_fftgrid(int nx,int ny,int nz)
{
  t_fftgrid *grid;
  
  snew(grid,1);
  grid->nx   = nx;
  grid->ny   = ny;
  grid->nz   = nz;
  grid->nxyz = nx*ny*nz;
  
  grid->la1  = ny;
  grid->la2  = nz;
  grid->nptr = nx*ny*nz;
  grid->la12 = grid->la1*grid->la2;

  snew(grid->ptr,grid->nptr);
  
  return grid;
}

void gmxfft3D(FILE *fp,bool bVerbose,t_fftgrid *grid,int dir)
{
  static bool bFirst=TRUE;
  static fftwnd_plan forward_plan,backward_plan;
  
  if (bFirst) {
    fprintf(fp,"Using the FFTW library (Fastest Fourier Transform in the West)\n");
    forward_plan  = fftw3d_create_plan(grid->nx,grid->ny,grid->nz,
				       FFTW_FORWARD,
				       /*FFTW_MEASURE | 
				       FFTW_USE_WISDOM |*/
				       FFTW_IN_PLACE);
    backward_plan = fftw3d_create_plan(grid->nx,grid->ny,grid->nz,
				       FFTW_BACKWARD,
				       /*FFTW_MEASURE | 
				       FFTW_USE_WISDOM |*/
				       FFTW_IN_PLACE);
    bFirst        = FALSE;
  }
  if (dir == FFTW_FORWARD)
    fftwnd(forward_plan, 1,(FFTW_COMPLEX *)grid->ptr,1,0,NULL,0,0);
  else if (dir == FFTW_BACKWARD)
    fftwnd(backward_plan,1,(FFTW_COMPLEX *)grid->ptr,1,0,NULL,0,0);
  else
    fatal_error(0,"Invalid direction for FFT: %d",dir);
}

void clear_fftgrid(t_fftgrid *grid)
{
  int      i,ngrid;
  t_fft_tp *ptr;
  
  ngrid = grid->nptr;
  ptr   = grid->ptr;
  
  for (i=0; (i<ngrid); i++) {
    ptr[i].re = ptr[i].im = 0;
  }
}

void unpack_fftgrid(t_fftgrid *grid,int *nx,int *ny,int *nz,
		    int *la1,int *la2,int *la12,t_fft_tp **ptr)
{
  *nx  = grid->nx;
  *ny  = grid->ny;
  *nz  = grid->nz;
  *la1 = grid->la1;
  *la2 = grid->la2;
  *la12= grid->la12;
  *ptr = grid->ptr;
}

void print_fftgrid(FILE *out,char *title,t_fftgrid *grid,real factor,char *pdb,
		   rvec box,bool bReal)
{
#define PDBTOL -1
  static char *pdbformat="%-6s%5d  %-4.4s%3.3s %c%4d    %8.3f%8.3f%8.3f%6.2f%6.2f\n";
  FILE     *fp;
  int      i,ix,iy,iz;
  real     fac=50.0,value;
  rvec     boxfac;
  int      nx,ny,nz,la1,la2,la12;
  t_fft_tp *ptr,g;
  
  if (pdb)
    fp = ffopen(pdb,"w");
  else
    fp = out;
  if (!fp)
    return;

  unpack_fftgrid(grid,&nx,&ny,&nz,&la1,&la2,&la12,&ptr);
    
  boxfac[XX] = fac*box[XX]/nx;
  boxfac[YY] = fac*box[YY]/ny;
  boxfac[ZZ] = fac*box[ZZ]/nz;
  
  if (pdb)
    fprintf(fp,"REMARK ");
  
  fprintf(fp,"Printing all non-zero %s elements of %s\n",
	  bReal ? "Real" : "Imaginary",title);
  for(i=ix=0; (ix<nx); ix++)
    for(iy=0; (iy<ny); iy++)
      for(iz=0; (iz<nz); iz++,i++) {
	g = ptr[INDEX(ix,iy,iz)];
	if (pdb) {
	  value = bReal ? g.re : g.im;
	  if (fabs(value) > PDBTOL)
	    fprintf(fp,pdbformat,"ATOM",i,"H","H",' ',
		    i,ix*boxfac[XX],iy*boxfac[YY],iz*boxfac[ZZ],
		    1.0,factor*value);
	} 
	else {
	  if ((fabs(g.re) > PDBTOL) || (fabs(g.im) > PDBTOL))
	    fprintf(fp,"%s[%2d][%2d][%2d] = %12.5e + i %12.5e%s\n",
		    title,ix,iy,iz,g.re*factor,g.im*factor,
		    (g.im != 0) ? " XXX" : "");
	}
      }
  fflush(fp);
#undef PDBTOL
}

/*****************************************************************
 * 
 * For backward compatibility (for testing the ewald code vs. PPPM etc)
 * some old grid routines are retained here.
 *
 ************************************************************************/

real ***mk_rgrid(int nx,int ny,int nz)
{
  real *ptr1;
  real **ptr2;
  real ***ptr3;
  int  i,j,n2,n3;
  
  snew(ptr1,nx*ny*nz);
  snew(ptr2,nx*ny);
  snew(ptr3,nx);
  
  n2=n3=0;
  for(i=0; (i<nx); i++) {
    ptr3[i]=&(ptr2[n2]);
    for(j=0; (j<ny); j++,n2++) { 
      ptr2[n2] = &(ptr1[n3]);
      n3 += nz;
    }
  }
  return ptr3;
}

void free_rgrid(real ***grid,int nx,int ny)
{
  int i;

  sfree(grid[0][0]);  
  for(i=0; (i<nx); i++) {
    sfree(grid[i]);
  }
  sfree(grid);
}

real print_rgrid(FILE *fp,char *title,int nx,int ny,int nz,real ***grid)
{
  int  ix,iy,iz;
  real g,gtot;
  
  gtot=0;
  if (fp)
    fprintf(fp,"Printing all non-zero real elements of %s\n",title);
  for(ix=0; (ix<nx); ix++)
    for(iy=0; (iy<ny); iy++)
      for(iz=0; (iz<nz); iz++) {
	g=grid[ix][iy][iz];
	if (fp && (g != 0))
	  fprintf(fp,"%s[%2d][%2d][%2d] = %12.5e\n",title,ix,iy,iz,g);
	gtot+=g;
      }
  return gtot;
}

void print_rgrid_pdb(char *fn,int nx,int ny,int nz,real ***grid)
{
  FILE *fp;
  int  ix,iy,iz,n,ig;
  real x,y,z,g;

  n=1;
  fp=ffopen(fn,"w");  
  for(ix=0; (ix<nx); ix++) {
    for(iy=0; (iy<ny); iy++) {
      for(iz=0; (iz<nz); iz++) {
	g=grid[ix][iy][iz];
	ig=g;
	if ((ig != 0) || (1)) {
	  x = 4*ix;
	  y = 4*iy;
	  z = 4*iz;
	  fprintf(fp,"ATOM  %5d  Na   Na     1    %8.3f%8.3f%8.3f%6.2f%6.2f\n",
		  n++,x,y,z,0.0,g);
	}
      }
    }
  }
  fclose(fp);
}

void clear_rgrid(int nx,int ny,int nz,real ***grid)
{
  int i,j,k;
  
  for(i=0; (i<nx); i++)
    for(j=0; (j<ny); j++)
      for(k=0; (k<nz); k++)
	grid[i][j][k] = 0;
}

void clear_cgrid(int nx,int ny,int nz,t_complex ***grid)
{
  int i,j,k;
  
  for(i=0; (i<nx); i++)
    for(j=0; (j<ny); j++)
      for(k=0; (k<nz); k++)
	grid[i][j][k] = cnul;
}

t_complex ***mk_cgrid(int nx,int ny,int nz)
{
  t_complex *ptr1;
  t_complex **ptr2;
  t_complex ***ptr3;
  int  i,j,n2,n3;
  
  snew(ptr1,nx*ny*nz);
  snew(ptr2,nx*ny);
  snew(ptr3,nx);
  
  n2=n3=0;
  for(i=0; (i<nx); i++) {
    ptr3[i]=&(ptr2[n2]);
    for(j=0; (j<ny); j++,n2++) { 
      ptr2[n2] = &(ptr1[n3]);
      n3 += nz;
    }
  }
  return ptr3;
}

void free_cgrid(t_complex ***grid,int nx,int ny)
{
  int i;

  sfree(grid[0][0]);
  for(i=0; (i<nx); i++) 
    sfree(grid[i]);
  sfree(grid);
}

t_complex print_cgrid(FILE *fp,char *title,int nx,int ny,int nz,
		      t_complex ***grid)
{
  int     ix,iy,iz;
  t_complex g,gtot;
  
  gtot=cnul;
  if (fp)
    fprintf(fp,"Printing all non-zero complex elements of %s\n",title);
  for(ix=0; (ix<nx); ix++)
    for(iy=0; (iy<ny); iy++)
      for(iz=0; (iz<nz); iz++) {
	g=grid[ix][iy][iz];
	if (fp  && ((g.re != 0) || (g.im != 0)))
	  fprintf(fp,"%s[%2d][%2d][%2d] = %12.5e + i %12.5e\n",
		  title,ix,iy,iz,g.re,g.im);
	gtot = cadd(gtot,g);
      }
  return gtot;
}

void print_cgrid_pdb(char *fn,int nx,int ny,int nz,t_complex ***grid)
{
  FILE *fp;
  int  ix,iy,iz,n;
  real x,y,z,g;

  n=1;
  fp=ffopen(fn,"w");  
  for(ix=0; (ix<nx); ix++) {
    for(iy=0; (iy<ny); iy++) {
      for(iz=0; (iz<nz); iz++) {
	g=grid[ix][iy][iz].re;
	if (g != 0) {
	  x = 4*ix;
	  y = 4*iy;
	  z = 4*iz;
	  fprintf(fp,"ATOM  %5d  Na   Na     1    %8.3f%8.3f%8.3f%6.2f%6.2f\n",
		  n++,x,y,z,0.0,g);
	}
      }
    }
  }
  fclose(fp);
}

