/*
 * This file is part of the GROMACS molecular simulation package.
 *
 * Copyright (c) 1991-2000, University of Groningen, The Netherlands.
 * Copyright (c) 2001-2004, The GROMACS development team.
 * Copyright (c) 2012,2014,2015, by the GROMACS development team, led by
 * Mark Abraham, David van der Spoel, Berk Hess, and Erik Lindahl,
 * and including many others, as listed in the AUTHORS file in the
 * top-level source directory and at http://www.gromacs.org.
 *
 * GROMACS is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * GROMACS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GROMACS; if not, see
 * http://www.gnu.org/licenses, or write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.
 *
 * If you want to redistribute modifications to GROMACS, please
 * consider that scientific software is very special. Version
 * control is crucial - bugs must be traceable. We will be happy to
 * consider code for inclusion in the official distribution, but
 * derived work must not be called official GROMACS. Details are found
 * in the README & COPYING files - if they are missing, get the
 * official version at http://www.gromacs.org.
 *
 * To help us fund GROMACS development, we humbly ask that you cite
 * the research papers on the package. Check out http://www.gromacs.org.
 */
#ifndef GMX_PBCUTIL_PBC_H
#define GMX_PBCUTIL_PBC_H

#include <stdio.h>

#include "gromacs/legacyheaders/types/inputrec.h"
#include "gromacs/math/vectypes.h"
#include "gromacs/utility/basedefinitions.h"
#include "gromacs/utility/real.h"

struct gmx_domdec_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum number of combinations of single triclinic box vectors
 * required to shift atoms that are within a brick of the size of
 * the diagonal of the box to within the maximum cut-off distance.
 */
#define MAX_NTRICVEC 12

/*! \brief Structure containing info on periodic boundary conditions */
typedef struct t_pbc {
    //! The PBC type
    int        ePBC;
    //! Number of dimensions in which PBC is exerted
    int        ndim_ePBC;
    /*! \brief Determines how to compute distance vectors.
     *
     *  Indicator of how to compute distance vectors, depending
     *  on PBC type (depends on ePBC and dimensions with(out) DD)
     *  and the box angles.
     */
    int        ePBCDX;
    /*! \brief Used for selecting which dimensions to use in PBC.
     *
     *  In case of 1-D PBC this indicates which dimension is used,
     *  in case of 2-D PBC this indicates the opposite
     */
    int        dim;
    //! The simulation box
    matrix     box;
    //! The lengths of the diagonal of the full box
    rvec       fbox_diag;
    //! Halve of the above
    rvec       hbox_diag;
    //! Negative of the above
    rvec       mhbox_diag;
    //! Maximum allowed cutoff squared for the box and PBC used
    real       max_cutoff2;
    /*! \brief Number of triclinic shift vectors.
     *
     *  Number of triclinic shift vectors depends on the skewedness
     *  of the box, that is mostly on the angles. For triclinic boxes
     *  we first take the closest image along each Cartesian dimension
     *  independently. When the resulting distance^2 is larger than
     *  max_cutoff2, up to ntric_vec triclinic shift vectors need to
     *  be tried. Because of the restrictions imposed on the unit-cell
     *  by GROMACS, ntric_vec <= MAX_NTRICVEC = 12.
     */
    int        ntric_vec;
    //! The triclinic shift vectors in grid cells. Internal use only.
    ivec       tric_shift[MAX_NTRICVEC];
    //!  The triclinic shift vectors in length units
    rvec       tric_vec[MAX_NTRICVEC];
} t_pbc;

#define TRICLINIC(box) (box[YY][XX] != 0 || box[ZZ][XX] != 0 || box[ZZ][YY] != 0)

#define NTRICIMG 14
#define NCUCVERT 24
#define NCUCEDGE 36

enum {
    ecenterTRIC, /* 0.5*(a+b+c)                  */
    ecenterRECT, /* (0.5*a[x],0.5*b[y],0.5*c[z]) */
    ecenterZERO, /* (0,0,0)                      */
    ecenterDEF = ecenterTRIC
};

struct t_graph;

/*! \brief Returns the number of dimensions that use pbc
 *
 * \param[in] ePBC The periodic boundary condition type
 * \return the number of dimensions that use pbc, starting at X
 */
int ePBC2npbcdim(int ePBC);

/*! \brief Return the number of bounded directories
 *
 * \param[in] ir The input record with MD parameters
 * \return the number of dimensions in which
 * the coordinates of the particles are bounded, starting at X.
 */
int inputrec2nboundeddim(const t_inputrec *ir);

/*! \brief Dump the contents of the pbc structure to the file
 *
 * \param[in] fp  The file pointer to write to
 * \param[in] pbc The periodic boundary condition information structure
 */
void dump_pbc(FILE *fp, t_pbc *pbc);

/*! \brief Check the box for consistency
 *
 * \param[in] ePBC The pbc identifier
 * \param[in] box  The box matrix
 * \return NULL if the box is supported by Gromacs.
 * Otherwise returns a string with the problem.
 * When ePBC=-1, the type of pbc is guessed from the box matrix.
 */
const char *check_box(int ePBC, matrix box);

/*! \brief Compute the maximum cutoff for the box

 * Returns the square of the maximum cut-off allowed for the box,
 * taking into account that the grid neighborsearch code and pbc_dx
 * only check combinations of single box-vector shifts.
 * \param[in] ePBC The pbc identifier
 * \param[in] box  The box matrix
 * \return the maximum cut-off.
 */
real max_cutoff2(int ePBC, matrix box);

/*! \brief Guess PBC typr
 *
 * Guesses the type of periodic boundary conditions using the box
 * \param[in] box  The box matrix
 * \return The pbc identifier
 */
int guess_ePBC(matrix box);

/*! \brief Corrects the box if necessary
 *
 * Checks for un-allowed box angles and corrects the box
 * and the integer shift vectors in the graph (if graph!=NULL) if necessary.
 * \param[in] fplog File for debug output
 * \param[in] step  The MD step number
 * \param[in] box   The simulation cell
 * \param[in] graph Information about molecular connectivity
 * \return TRUE when the box was corrected.
 */
gmx_bool correct_box(FILE *fplog, int step, tensor box, struct t_graph *graph);

/*! \brief Returns the number of degrees of freedom in center of mass motion
 *
 * \param[in] ir the inputrec structure
 * \return the number of degrees of freedom of the center of mass
 */
int ndof_com(t_inputrec *ir);

/*! \brief Initiate the periodic boundary condition algorithms.
 *
 * pbc_dx will not use pbc and return the normal difference vector
 * when one or more of the diagonal elements of box are zero.
 * When ePBC=-1, the type of pbc is guessed from the box matrix.
 * \param[inout] pbc The pbc information structure
 * \param[in] ePBC The PBC identifier
 * \param[in] box  The box tensor
 */
void set_pbc(t_pbc *pbc, int ePBC, matrix box);

/*! \brief Initiate the periodic boundary condition algorithms.
 *
 * As set_pbc, but additionally sets that correct distances can
 * be obtained using (combinations of) single box-vector shifts.
 * Should be used with pbc_dx_aiuc.
 * If dd!=NULL pbc is not used for directions
 * with dd->nc[i]==1 with bSingleDir==TRUE or
 * with dd->nc[i]<=2 with bSingleDir==FALSE.
 * \param[inout] pbc The pbc information structure
 * \param[in] ePBC       The PBC identifier
 * \param[in] dd         The domain decomposition struct
 * \param[in] bSingleDir TRUE if DD communicates only in one direction along dimensions
 * \param[in] box        The box tensor
 * \return the pbc structure when pbc operations are required, NULL otherwise.
 */
t_pbc *set_pbc_dd(t_pbc *pbc, int ePBC,
                  struct gmx_domdec_t *dd, gmx_bool bSingleDir, matrix box);

/*! \brief Compute distance with PBC
 *
 * Calculate the correct distance vector from x2 to x1 and put it in dx.
 * set_pbc must be called before ever calling this routine.
 *
 * Note that for triclinic boxes that do not obey the GROMACS unit-cell
 * restrictions, pbc_dx and pbc_dx_aiuc will not correct for PBC.
 * \param[inout] pbc The pbc information structure
 * \param[in]    x1  Coordinates for particle 1
 * \param[in]    x2  Coordinates for particle 2
 * \param[out]   dx  Distance vector
 */
void pbc_dx(const t_pbc *pbc, const rvec x1, const rvec x2, rvec dx);

/*! \brief Compute distance vector for simple PBC types
 *
 * Calculate the correct distance vector from x2 to x1 and put it in dx,
 * This function can only be used when all atoms are in the rectangular
 * or triclinic unit-cell.
 * set_pbc_dd or set_pbc must be called before ever calling this routine.
 * \param[inout] pbc The pbc information structure
 * \param[in]    x1  Coordinates for particle 1
 * \param[in]    x2  Coordinates for particle 2
 * \param[out]   dx  Distance vector
 * \return the ishift required to shift x1 at closest distance to x2;
 * i.e. if 0<=ishift<SHIFTS then x1 - x2 + shift_vec[ishift] = dx
 * (see calc_shifts below on how to obtain shift_vec)
 */
int pbc_dx_aiuc(const t_pbc *pbc, const rvec x1, const rvec x2, rvec dx);

/*\brief Compute distance with PBC
 *
 * As pbc_dx, but for double precision vectors.
 * set_pbc must be called before ever calling this routine.
 * \param[inout] pbc The pbc information structure
 * \param[in]    x1  Coordinates for particle 1
 * \param[in]    x2  Coordinates for particle 2
 * \param[out]   dx  Distance vector
 */
void pbc_dx_d(const t_pbc *pbc, const dvec x1, const dvec x2, dvec dx);

/*! \brief Calculate the distance between xi and xj for a rectangular box.
 *
 * It is assumed that rlong2 is scaled the same way as the ivecs xi and xj.
 * \param[in]  xi     Box index
 * \param[in]  xj     Box index
 * \param[in]  box    The box of grid cells
 * \param[in]  rlong2 Cutoff squared
 * \param[out] shift  The shift code
 * \param[out] r2     The distance (squared???)
 * \return TRUE when the distance is SMALLER than rlong2
 */
gmx_bool image_rect(ivec xi, ivec xj, imatrix box,
                    real rlong2, int *shift, real *r2);

/*! \brief Calculate the distance between xi and xj for a triclinic box.
 *
 * It is assumed that rlong2 is scaled the same way as the ivecs xi and xj.
 * \param[in]  xi     Box index
 * \param[in]  xj     Box index
 * \param[in]  box    Matrix of box grid cells
 * \param[in]  rlong2 Cutoff squared
 * \param[out] shift  The shift code
 * \param[out] r2     The distance (squared???)
 * \return TRUE when the distance is SMALLER than rlong2
 */
gmx_bool image_tri(ivec xi, ivec xj, imatrix box,
                   real rlong2, int *shift, real *r2);

/*! \brief Compute distance vector when using cylindrical cutoff
 *
 * Calculate the distance between xi and xj for a rectangular box
 * using a cylindric cutoff for long-range only.
 * It is assumed that rlong2 is scaled the same way as the ivecs xi and xj.
 * \param[in]  xi       Box index
 * \param[in]  xj       Box index
 * \param[in]  box_size Number of box grid cells
 * \param[in]  rlong2   Cutoff squared
 * \param[out] shift    The shift code
 * \param[out] r2       The distance (squared???)
 * \return TRUE when the distance is SMALLER than rlong2 (in X and Y dir)
 */
gmx_bool image_cylindric(ivec xi, ivec xj, ivec box_size, real rlong2,
                         int *shift, real *r2);

/*! \brief Computes shift vectors
 *
 * This routine calculates ths shift vectors necessary to use the
 * neighbor searching routine.
 * \param[in]  box       The simulation box
 * \param[out] shift_vec The shifting vectors
 */
void calc_shifts(matrix box, rvec shift_vec[]);

/*! \brief Calculates the center of the box.
 *
 * See the description for the enum ecenter above.
 * \param[in]  ecenter    Description of center type
 * \param[in]  box        The simulation box
 * \param[out] box_center The center of the box
 */
void calc_box_center(int ecenter, matrix box, rvec box_center);

/*! \brief Calculates the NTRICIMG box images
 *
 * \param[in]  box The simulation box
 * \param[out] img The triclinic box images
 */
void calc_triclinic_images(matrix box, rvec img[]);

/*! \brief Calculates the NCUCVERT vertices of a compact unitcell
 *
 * \param[in]  ecenter The center type
 * \param[in]  box     The simulation box
 * \param[out] vert    The vertices
 */
void calc_compact_unitcell_vertices(int ecenter, matrix box,
                                    rvec vert[]);

/*! \brief Compute unitcell edges
 *
 * \return an array of unitcell edges of length NCUCEDGE*2,
 * this is an index in vert[], which is calculated by calc_unitcell_vertices.
 * The index consists of NCUCEDGE pairs of vertex indices.
 * The index does not change, so it needs to be retrieved only once.
 */
int *compact_unitcell_edges(void);

/*! \brief Parallellizes put_atoms_box
 *
 * This wrapper function around put_atoms_in_box() with the ugly manual
 * workload splitting is needed to avoid silently introducing multithreading
 * in tools.
 * \param[in]    ePBC   The pbc type
 * \param[in]    box    The simulation box
 * \param[in]    natoms The number of atoms
 * \param[inout] x      The coordinates of the atoms
 */
void put_atoms_in_box_omp(int ePBC, matrix box, int natoms, rvec x[]);


/*! \brief Put atoms inside the simulations box
 *
 * These routines puts ONE or ALL atoms in the box, not caring
 * about charge groups!
 * Also works for triclinic cells.
 * \param[in]    ePBC   The pbc type
 * \param[in]    box    The simulation box
 * \param[in]    natoms The number of atoms
 * \param[inout] x      The coordinates of the atoms
 */
void put_atoms_in_box(int ePBC, matrix box, int natoms, rvec x[]);

/*! \brief Put atoms inside triclinic box
 *
 * This puts ALL atoms in the triclinic unit cell, centered around the
 * box center as calculated by calc_box_center.
 * \param[in]    ecenter The pbc center type
 * \param[in]    box     The simulation box
 * \param[in]    natoms  The number of atoms
 * \param[inout] x       The coordinates of the atoms
 */
void put_atoms_in_triclinic_unitcell(int ecenter, matrix box,
                                     int natoms, rvec x[]);

/*! \brief Put atoms inside the unitcell
 *
 * This puts ALL atoms at the closest distance for the center of the box
 * as calculated by calc_box_center.
 * When ePBC=-1, the type of pbc is guessed from the box matrix.
 * \param[in]    ePBC    The pbc type
 * \param[in]    ecenter The pbc center type
 * \param[in]    box     The simulation box
 * \param[in]    natoms  The number of atoms
 * \param[inout] x       The coordinates of the atoms
 */
void put_atoms_in_compact_unitcell(int ePBC, int ecenter,
                                   matrix box,
                                   int natoms, rvec x[]);

#ifdef __cplusplus
}
#endif

#endif
