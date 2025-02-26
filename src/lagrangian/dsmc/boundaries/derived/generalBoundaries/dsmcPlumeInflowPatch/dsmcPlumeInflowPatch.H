/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 1991-2007 OpenCFD Ltd.
     \\/     M anipulation  |
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the
    Free Software Foundation; either version 2 of the License, or (at your
    option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM; if not, write to the Free Software Foundation,
    Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

Class
    dsmcPlumeInflowPatch

Description

    Fully deletion boundary patch: atomisticMolecules get deleted instantly upon
    collision with a face on the boundary. (aka vacuum boundary condition)

    Selective deletion of one specie is possible. Other species get
    specularly reflected.

SourceFiles
    dsmcPlumeInflowPatch.C


\*---------------------------------------------------------------------------*/

#ifndef dsmcPlumeInflowPatch_H
#define dsmcPlumeInflowPatch_H

#include "dsmcGeneralBoundary.H"
#include "vector.H"
#include "polyMesh.H"
#include "volFields.H"
#include "dsmcCloud.H"
#include "distribution.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

/*---------------------------------------------------------------------------*\
                           Class dsmcPlumeInflowPatch Declaration
\*---------------------------------------------------------------------------*/

class dsmcPlumeInflowPatch
:
	public dsmcGeneralBoundary
{

private:

    // Private data
    dictionary propsDict_;

    labelList typeIds_;
    
    scalar axialVelocity_; 
    
    scalar radialVelocity_;
    
    scalar origin_;

    // list of (one value) mass densities corresponding to the type Ids
    scalarField numberDensities_;

    // list of species 
    List <scalarField> accumulatedParcelsToInsert_;
    
    vectorField inletVelocities_;

    //- boundary temperatures
    scalar translationalT_;
    scalar rotationalT_;
    scalarList vibrationalTemperatures_;
    scalar electronicT_;

    void setProperties();

public:

    //- Runtime type information
    TypeName("dsmcPlumeInflowPatch");

    // Constructors

        //- Construct from components
        dsmcPlumeInflowPatch
        (
            Time& t,
            const polyMesh& mesh,
            dsmcCloud& cloud,
            const dictionary& dict
        );

    // Destructor

        ~dsmcPlumeInflowPatch();


    // Member Functions

        void initialConfiguration();

        //- calculate any required properties
        void calculateProperties();

        //- control the mols
        void controlParcelsBeforeMove();
        void controlParcelsBeforeCollisions();
        void controlParcelsAfterCollisions();

        //- output data
        void output
        (
            const fileName& fixedPathName,
            const fileName& timePath
        );

        //- update properties from a modified dictionary
        void updateProperties(const dictionary&);
        
        //reset fields if mesh has changed
    void setNewBoundaryFields();



        // Access

};


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
