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

Description

\*---------------------------------------------------------------------------*/

#include "dsmcYePressureOutletCalculatedMolarFraction.H"
#include "addToRunTimeSelectionTable.H"
#include "fvc.H"

// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

using namespace Foam::constant::mathematical;

namespace Foam
{

defineTypeNameAndDebug(dsmcYePressureOutletCalculatedMolarFraction, 0);

addToRunTimeSelectionTable(dsmcGeneralBoundary, dsmcYePressureOutletCalculatedMolarFraction, dictionary);



// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

// Construct from components
dsmcYePressureOutletCalculatedMolarFraction::dsmcYePressureOutletCalculatedMolarFraction
(
    Time& t,
    const polyMesh& mesh,
    dsmcCloud& cloud,
    const dictionary& dict
)
:
    dsmcGeneralBoundary(t, mesh, cloud, dict),
    propsDict_(dict.subDict(typeName + "Properties")),
    typeIds_(),
    moleFractions_(),
    outletPressure_(),
    cellWidth_(),
    cellVolume_(faces_.size(), scalar(0.0)),
    accumulatedParcelsToInsert_(),
    outletVelocity_(faces_.size(), vector::zero),
    outletNumberDensity_(faces_.size(), scalar(0.0)),
    outletMassDensity_(faces_.size(), scalar(0.0)),
    outletTranslationalTemperature_(faces_.size(), scalar(300.0)),
    outletRotationalTemperature_(faces_.size(), scalar(300.0)),
    outletVibrationalTemperature_(faces_.size(), scalar(300.0)),
    totalMomentum_(faces_.size(), vector::zero),
    totalMass_(faces_.size(), scalar(0.0)),
    totalMassDensity_(faces_.size(), scalar(0.0)),
    totalNumberDensity_(faces_.size(), scalar(0.0)),
    totalPressure_(faces_.size(), scalar(0.0)),
    totalRotationalEnergy_(faces_.size(), scalar(0.0)),
    totalRotationalDof_(faces_.size(), scalar(0.0)),
    totalVibrationalEnergy_(),
    vibT_(),
    vDof_(),
    velSqrMeanX_(faces_.size(), scalar(0.0)),
    velSqrMeanY_(faces_.size(), scalar(0.0)),
    velSqrMeanZ_(faces_.size(), scalar(0.0)),
    velMeanSqrX_(faces_.size(), scalar(0.0)),
    velMeanSqrY_(faces_.size(), scalar(0.0)),
    velMeanSqrZ_(faces_.size(), scalar(0.0)),
    totalVelSqrMeanX_(faces_.size(), scalar(0.0)),
    totalVelSqrMeanY_(faces_.size(), scalar(0.0)),
    totalVelSqrMeanZ_(faces_.size(), scalar(0.0)),
    totalVelMeanSqrX_(faces_.size(), scalar(0.0)),
    totalVelMeanSqrY_(faces_.size(), scalar(0.0)),
    totalVelMeanSqrZ_(faces_.size(), scalar(0.0)),
    nTotalParcels_(faces_.size(), scalar(0.0)),
    nTotalParcelsInt_(faces_.size(), scalar(0.0)),
    nTotalParcelsSpecies_(),
    nTimeSteps_(scalar(0.0))
{
    writeInTimeDir_ = false;
    writeInCase_ = true;

    setProperties();
        
    forAll(cellVolume_, c)
    {
        cellVolume_[c] = mesh_.cellVolumes()[cells_[c]];  //get volume of each boundary cell
    }

}


// * * * * * * * * * * * * * * * * Destructor  * * * * * * * * * * * * * * * //

dsmcYePressureOutletCalculatedMolarFraction::~dsmcYePressureOutletCalculatedMolarFraction()
{}



// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //
void dsmcYePressureOutletCalculatedMolarFraction::initialConfiguration()
{}

void dsmcYePressureOutletCalculatedMolarFraction::calculateProperties()
{

}

void dsmcYePressureOutletCalculatedMolarFraction::controlParcelsBeforeMove()
{   
    Random& rndGen = cloud_.rndGen();
    
    label nTotalParcelsAdded = 0;
    label nTotalParcelsToBeAdded = 0;

    forAll(accumulatedParcelsToInsert_, iD)   // I Added.
    {
        // loop over all faces of the patch
        forAll(accumulatedParcelsToInsert_[iD], f)
        {
            vector faceVelocity = outletVelocity_[f];
            scalar faceTranslationalTemperature = outletTranslationalTemperature_[f];
            scalar faceRotationalTemperature = outletRotationalTemperature_[f];
            scalar faceVibrationalTemperature = outletVibrationalTemperature_[f];
            const label& faceI = faces_[f];
            const label& cellI = cells_[f];
            const vector& fC = mesh_.faceCentres()[faceI];
            const vector& sF = mesh_.faceAreas()[faces_[f]];
            scalar fA = mag(sF);

            List<tetIndices> faceTets = polyMeshTetDecomposition::faceTetIndices
            (
                mesh_,
                faceI,
                cellI
            );

        //         Cumulative triangle area fractions
            List<scalar> cTriAFracs(faceTets.size(), 0.0);

            scalar previousCummulativeSum = 0.0;

            forAll(faceTets, triI)
            {
                const tetIndices& faceTetIs = faceTets[triI];

                cTriAFracs[triI] =
                    faceTetIs.faceTri(mesh_).mag()/fA
                    + previousCummulativeSum;

                previousCummulativeSum = cTriAFracs[triI];
            }

        //         Force the last area fraction value to 1.0 to avoid any
        //         rounding/non-flat face errors giving a value < 1.0
            cTriAFracs.last() = 1.0;

        //         Normal unit vector *negative* so normal is pointing into the
        //         domain
            vector n = sF;
            n /= -mag(n);

        //         Wall tangential unit vector. Use the direction between the
        //         face centre and the first vertex in the list
            vector t1 = fC - mesh_.points()[mesh_.faces()[faceI][0]]; 
            t1 /= mag(t1);

        //         Other tangential unit vector.  Rescaling in case face is not
        //         flat and n and t1 aren't perfectly orthogonal
            vector t2 = n^t1;
            t2 /= mag(t2);

            label nParcelsToInsert = label(accumulatedParcelsToInsert_[iD][f]);
            
            if ((nParcelsToInsert - accumulatedParcelsToInsert_[iD][f]) > rndGen.sample01<scalar>())
            {
                nParcelsToInsert++;
            }
            
            accumulatedParcelsToInsert_[iD][f] -= nParcelsToInsert; //remainder has been set

            nTotalParcelsToBeAdded += nParcelsToInsert;

            const label& typeId = typeIds_[iD];
            scalar mass = cloud_.constProps(typeId).mass();

            for (label i = 0; i < nParcelsToInsert; i++)
            {
                // Choose a triangle to insert on, based on their relative
                // area

                scalar triSelection = rndGen.sample01<scalar>();

                // Selected triangle
                label selectedTriI = -1;

                forAll(cTriAFracs, triI)
                {
                    selectedTriI = triI;

                    if (cTriAFracs[triI] >= triSelection)
                    {
                        break;
                    }
                }

                // Randomly distribute the points on the triangle.

                const tetIndices& faceTetIs = faceTets[selectedTriI];

                point p = faceTetIs.faceTri(mesh_).randomPoint(rndGen);
                
                // Velocity generation
                scalar mostProbableSpeed
                (
                    cloud_.maxwellianMostProbableSpeed
                    (
                        faceTranslationalTemperature,
                        mass
                    )
                );
                    
                scalar sCosTheta = (faceVelocity & n)/mostProbableSpeed;

                // Coefficients required for Bird eqn 12.5
                scalar uNormProbCoeffA = sCosTheta + sqrt(sqr(sCosTheta) + 2.0);

                scalar uNormProbCoeffB =
                    0.5*
                    (
                    1.0
                    + sCosTheta*(sCosTheta - sqrt(sqr(sCosTheta) + 2.0))
                    );

                // Equivalent to the QA value in Bird's DSMC3.FOR
                scalar randomScaling = 3.0;

                if (sCosTheta < -3)
                {
                    randomScaling = mag(sCosTheta) + 1;
                }

                scalar P = -1;

                // Normalised candidates for the normal direction velocity
                // component
                scalar uNormal;
                scalar uNormalThermal;

                if(abs(faceVelocity & n) > VSMALL)
                {
                    // Select a velocity using Bird eqn 12.5
                    do
                    {
                        uNormalThermal =
                            randomScaling*(2.0*rndGen.sample01<scalar>() - 1);

                        uNormal = uNormalThermal + sCosTheta;

                        if (uNormal < 0.0)
                        {
                            P = -1;
                        }
                        else
                        {
                            P = 2.0*uNormal/uNormProbCoeffA
                                *exp(uNormProbCoeffB - sqr(uNormalThermal));
                        }

                    } while (P < rndGen.sample01<scalar>());
                }
                else
                {
                    uNormal = sqrt(-log(rndGen.sample01<scalar>()));
                }
                
                vector U =
                    sqrt(physicoChemical::k.value()*faceTranslationalTemperature/mass)
                    *(
                        rndGen.GaussNormal<scalar>()*t1
                        + rndGen.GaussNormal<scalar>()*t2
                    )
                    + (t1 & faceVelocity)*t1
                    + (t2 & faceVelocity)*t2
                    + mostProbableSpeed*uNormal*n;

                scalar ERot = cloud_.equipartitionRotationalEnergy
                (
                    faceRotationalTemperature,
                    cloud_.constProps(typeId).rotationalDegreesOfFreedom()
                );
                
                labelList vibLevel = cloud_.equipartitionVibrationalEnergyLevel
                (
                    faceVibrationalTemperature,
                    cloud_.constProps(typeId).nVibrationalModes(),
                    typeId
                );
                
                label ELevel = 0.0;
                
                label newParcel = patchId();
                
                const scalar& RWF = cloud_.coordSystem().recalculateRWF(cellI);
              
                cloud_.addNewParcel
                (
                    p,
                    U,
                    RWF,
                    ERot,
                    ELevel,
                    cellI,
                    faces_[f],
                    faceTetIs.tetPt(),
                    typeId,
                    newParcel,
                    0,
                    vibLevel
                );

                nTotalParcelsAdded++;
            }
        }
    }

    Info<< "dsmcYePressureOutletCalculatedMolarFraction target parcels to insert: " << nTotalParcelsToBeAdded 
        <<", number of inserted parcels: " << nTotalParcelsAdded
        << endl;
}

void dsmcYePressureOutletCalculatedMolarFraction::controlParcelsBeforeCollisions()
{   
}

void dsmcYePressureOutletCalculatedMolarFraction::controlParcelsAfterCollisions()
{    
    nTimeSteps_ += 1.0;
    
    const scalar sqrtPi = sqrt(pi);
           
    scalarField molecularMass(cells_.size(), 0.0);
    scalarField molarcontantPressureSpecificHeat(cells_.size(), 0.0);
    scalarField molarcontantVolumeSpecificHeat(cells_.size(), 0.0);
    scalarField gasConstant(cells_.size(), 0.0);
    scalarField gamma(cells_.size(), 0.0);

    forAll(cells_, c)
    {     
        forAll(moleFractions_, j)  
        {
            const label& typeId = typeIds_[j];

            molecularMass[c] += cloud_.constProps(typeId).mass()*moleFractions_[j][c];
            molarcontantPressureSpecificHeat[c] += (5.0 + cloud_.constProps(typeId).rotationalDegreesOfFreedom())*moleFractions_[j][c];
            molarcontantVolumeSpecificHeat[c] += (3.0 + cloud_.constProps(typeId).rotationalDegreesOfFreedom())*moleFractions_[j][c];
        } 

        gasConstant[c] = physicoChemical::k.value()/molecularMass[c]; // R = k/m

        gamma[c] = molarcontantPressureSpecificHeat[c]/molarcontantVolumeSpecificHeat[c];   // I Added.
    } 

    // calculate properties in cells attached to each boundary face
    
    vectorField momentum(faces_.size(), vector::zero);
    scalarField mass(faces_.size(), scalar(0.0));
    vectorField cellVelocity(faces_.size(), vector::zero);
    vector outletAverageVelocity(vector::zero);
    scalarField nParcels(faces_.size(), scalar(0.0));
    scalarField nParcelsInt(faces_.size(), scalar(0.0));
    scalarField velSqrMeanX(faces_.size(), scalar(0.0));
    scalarField velSqrMeanY(faces_.size(), scalar(0.0));
    scalarField velSqrMeanZ(faces_.size(), scalar(0.0));
    scalarField velMeanSqrX(faces_.size(), scalar(0.0));
    scalarField velMeanSqrY(faces_.size(), scalar(0.0));
    scalarField velMeanSqrZ(faces_.size(), scalar(0.0));
    scalarField rotationalEnergy(faces_.size(), scalar(0.0));
    scalarField rotationalDof(faces_.size(), scalar(0.0));
    scalarField vibTxvDof(faces_.size(), scalar(0.0));
    scalarField vDoF(faces_.size(), scalar(0.0));
    scalarField translationalTemperature(faces_.size(), scalar(0.0));
    scalarField rotationalTemperature(faces_.size(), scalar(0.0));
    scalarField vibrationalTemperature(faces_.size(), scalar(0.0));
    scalarField overallTemperature(faces_.size(), scalar(0.0));
    scalarField numberDensity(faces_.size(), scalar(0.0));
    scalarField massDensity(faces_.size(), scalar(0.0));
    scalarField pressure(faces_.size(), scalar(0.0));
    scalarField speedOfSound(faces_.size(), scalar(0.0));
    scalarField velocityCorrection(faces_.size(), scalar(0.0));
    scalarField massDensityCorrection(faces_.size(), scalar(0.0));

    const List<DynamicList<dsmcParcel*> >& cellOccupancy = cloud_.cellOccupancy();
    
    forAll(cells_, c)
    {
        const label celli = cells_[c];
        
        const List<dsmcParcel*>& parcelsInCell = cellOccupancy[celli];
        
        const scalar deltaT = cloud_.deltaTValue(celli);
                  
        forAll(parcelsInCell, pIC)
        {
            dsmcParcel* p = parcelsInCell[pIC];
                        
            momentum[c] += cloud_.nParticles(celli)*cloud_.constProps(p->typeId()).mass()*p->U();
            mass[c] += cloud_.nParticles(celli)*cloud_.constProps(p->typeId()).mass();
            nParcels[c] += 1.0;
            rotationalEnergy[c] += p->ERot();
            rotationalDof[c] += cloud_.constProps(p->typeId()).rotationalDegreesOfFreedom();
            velSqrMeanX[c] += sqr(p->U().x());
            velSqrMeanY[c] += sqr(p->U().y());
            velSqrMeanZ[c] += sqr(p->U().z());
            velMeanSqrX[c] += p->U().x();
            velMeanSqrY[c] += p->U().y();
            velMeanSqrZ[c] += p->U().z();
            
            if(cloud_.constProps(p->typeId()).rotationalDegreesOfFreedom() > VSMALL)
            {
                nParcelsInt[c] += 1.0;
            }
        } 
        
        nTotalParcels_[c] += nParcels[c];
        
        nTotalParcelsInt_[c] += nParcelsInt[c];
        
        totalMomentum_[c] += momentum[c];
        
        totalMass_[c] += mass[c]; 
        
        totalRotationalEnergy_[c] += rotationalEnergy[c];
        
        totalRotationalDof_[c] += rotationalDof[c];
               
        massDensity[c] = totalMass_[c] / (cellVolume_[c]*nTimeSteps_);
        
        numberDensity[c] = massDensity[c]/molecularMass[c];
                
        if(nTotalParcels_[c] > 1)
        {
            velSqrMeanX_[c] += velSqrMeanX[c];
            velSqrMeanY_[c] += velSqrMeanY[c];
            velSqrMeanZ_[c] += velSqrMeanZ[c];
            velMeanSqrX_[c] += velMeanSqrX[c];
            velMeanSqrY_[c] += velMeanSqrY[c];
            velMeanSqrZ_[c] += velMeanSqrZ[c];
            
            totalVelSqrMeanX_[c] = velSqrMeanX_[c]/nTotalParcels_[c];
            totalVelSqrMeanY_[c] = velSqrMeanY_[c]/nTotalParcels_[c];
            totalVelSqrMeanZ_[c] = velSqrMeanZ_[c]/nTotalParcels_[c];
            totalVelMeanSqrX_[c] = sqr(velMeanSqrX_[c]/nTotalParcels_[c]);
            totalVelMeanSqrY_[c] = sqr(velMeanSqrY_[c]/nTotalParcels_[c]);
            totalVelMeanSqrZ_[c] = sqr(velMeanSqrZ_[c]/nTotalParcels_[c]);
                    
            translationalTemperature[c] = (molecularMass[c])*(1.0/(1.0*physicoChemical::k.value()))
                        *(
                                totalVelSqrMeanX_[c]+totalVelSqrMeanY_[c]+totalVelSqrMeanZ_[c]
                                -totalVelMeanSqrX_[c]-totalVelMeanSqrY_[c]-totalVelMeanSqrZ_[c]
                            );
                        
            if(totalRotationalDof_[c] > VSMALL)
            {
                rotationalTemperature[c] = (2.0/physicoChemical::k.value())*(totalRotationalEnergy_[c]/totalRotationalDof_[c]);
            }
            else
            {
                rotationalTemperature[c] = 0.0;
            }

//             forAll(totalVibrationalEnergy_, iD)
//             {
//                 const List<dsmcParcel*>& parcelsInCell = cellOccupancy[cells_[c]];
// 
//                 forAll(parcelsInCell, pIC)
//                 {
//                     dsmcParcel* p = parcelsInCell[pIC];
// 
//                     if(p->typeId() == typeIds_[iD])
//                     {
//                         totalVibrationalEnergy_[iD][c] += p->vibLevel()*physicoChemical::k.value()*cloud_.constProps(p->typeId()).thetaV();
//                         nTotalParcelsSpecies_[iD][c] += 1.0;
//                     }
//                 }
//             }
//             
//             forAll(totalVibrationalEnergy_, iD)
//             {
//                 if(totalVibrationalEnergy_[iD][c] > VSMALL)
//                 {   
//                     const scalar& thetaV = cloud_.constProps(typeIds_[iD]).thetaV();
//                     
//                     scalar vibrationalEMean = (totalVibrationalEnergy_[iD][c]/nTotalParcelsSpecies_[iD][c]);
//                     scalar iMean = vibrationalEMean/(physicoChemical::k.value()*thetaV);
//                     
//                     vibT_[iD][c] = thetaV / log(1.0 + (1.0/iMean));
//                     vDof_[iD][c] = (2.0*thetaV/vibT_[iD][c]) / (exp(thetaV/vibT_[iD][c]) - 1.0);
//                     
//                     scalar fraction = nTotalParcelsSpecies_[iD][c]/nTotalParcelsInt_[c];
//                     
//                     vDoF[c] += fraction*vDof_[iD][c];
//                     
//                     vibrationalTemperature[c] += fraction*vibT_[iD][c];
//                 } 
//             }
            
            scalar nRotDof = 0.0;
                
            if(nTotalParcels_[c] > VSMALL)
            {
                nRotDof = totalRotationalDof_[c]/nTotalParcels_[c];
            }
            
            overallTemperature[c] = (
                                        (3.0*translationalTemperature[c]) 
                                        + (nRotDof*rotationalTemperature[c])
                                        + (vDoF[c]*vibrationalTemperature[c])
                                    )
                        / (3.0 + nRotDof + vDoF[c]);
                                        
            pressure[c] = numberDensity[c]*physicoChemical::k.value()*translationalTemperature[c];
                                
            massDensityCorrection[c] = (2.0*deltaT*(outletPressure_ - pressure[c])) / (massDensity[c]*cellWidth_);
            
            outletMassDensity_[c] = massDensity[c] /*+ massDensityCorrection[c]*/; // Liou and Fang, 2000, equation 17
            
            outletNumberDensity_[c] = outletMassDensity_[c] / molecularMass[c]; // Liou and Fang, 2000, equation 26 STEP 1
            
            outletTranslationalTemperature_[c] = outletPressure_ / (gasConstant[c]*outletMassDensity_[c]);
            
            outletRotationalTemperature_[c] = rotationalTemperature[c];
            
            outletVibrationalTemperature_[c] = vibrationalTemperature[c];
            
            velocityCorrection[c] = (2.0*deltaT*(pressure[c] - outletPressure_)) / (massDensity[c]*cellWidth_); //velocity correction for each boundary cell
                    
            label faceI = faces_[c];
            vector n = mesh_.faceAreas()[faceI];        
            n /= mag(n);
            
            vector outletFaceNormal(vector::zero);
            
            outletFaceNormal.x() = abs(n.x());
            outletFaceNormal.y() = abs(n.y());
            outletFaceNormal.z() = abs(n.z());
            
            outletVelocity_[c].x() = totalMomentum_[c].x()/totalMass_[c]; //equation 4 from Wang (2004)
            outletVelocity_[c].y() = totalMomentum_[c].y()/totalMass_[c];
            outletVelocity_[c].z() = totalMomentum_[c].z()/totalMass_[c];
            
            outletVelocity_[c] += velocityCorrection[c]*outletFaceNormal;
        }
    }
    
    forAll(moleFractions_, iD)
    {
        forAll(moleFractions_[iD], c)
        {
            moleFractions_[iD][c] = (nTotalParcelsSpecies_[iD][c]*1.0)/nTotalParcels_[c];
        }
    }
        
    if(faces_.size() > VSMALL)
    {
        Pout << "dsmcYePressureOutletCalculatedMolarFraction outlet velocity correction = " << velocityCorrection[(faces_.size()/2)] << endl;
    }
    
    if(faces_.size() > VSMALL)
    {
        Pout << "dsmcYePressureOutletCalculatedMolarFraction outlet velocity = " << outletVelocity_[(faces_.size()/2)].x() << endl; // just for output purposes, not used in any calculations here    
    }
    
    forAll(accumulatedParcelsToInsert_, iD)
    {
        const label& typeId = typeIds_[iD];
        
        forAll(accumulatedParcelsToInsert_[iD], f)
        {
            const label& faceI = faces_[f];
            const vector& sF = mesh_.faceAreas()[faceI];
            const scalar fA = mag(sF);
            
            const scalar deltaT = cloud_.deltaTValue(mesh_.boundaryMesh()[patchId_].faceCells()[faceI]);

            scalar mass = cloud_.constProps(typeId).mass();              

            scalar mostProbableSpeed
            (
                cloud_.maxwellianMostProbableSpeed
                (
                    outletTranslationalTemperature_[f],
                    mass
                )
            );

             // Dotting boundary velocity with the face unit normal
            // (which points out of the domain, so it must be
            // negated), dividing by the most probable speed to form
            // molecularSpeedRatio * cosTheta
            
            scalar sCosTheta = (outletVelocity_[f] & -sF/fA )/mostProbableSpeed;
            
            //const scalar& RWF = cloud_.coordSystem().pRWF(patchId_, f);
            
            // From Bird eqn 4.22
            accumulatedParcelsToInsert_[iD][f] += 
                moleFractions_[iD][f]*
                (
                    fA*outletNumberDensity_[f]*deltaT*mostProbableSpeed
                    *
                    (
                        exp(-sqr(sCosTheta)) + sqrtPi*sCosTheta*(1 + erf(sCosTheta))
                    )
                )
                /(2.0*sqrtPi*cloud_.nParticles(patchId_, f));
        } 
    }

}

void dsmcYePressureOutletCalculatedMolarFraction::output
(
    const fileName& fixedPathName,
    const fileName& timePath
)
{
}

void dsmcYePressureOutletCalculatedMolarFraction::updateProperties(const dictionary& newDict)
{
    //- the main properties should be updated first
    updateBoundaryProperties(newDict);

    setProperties();
}

void dsmcYePressureOutletCalculatedMolarFraction::setProperties()
{
	outletPressure_ = readScalar(propsDict_.lookup("outletPressure"));
    
    cellWidth_ = readScalar(propsDict_.lookup("cellWidth"));
        
    const List<word> molecules (propsDict_.lookup("typeIds"));

    if(molecules.size() == 0)
    {
        FatalErrorIn("dsmcYePressureOutletCalculatedMolarFraction::setProperties()")
            << "Cannot have zero typeIds being inserd." << nl << "in: "
            << mesh_.time().system()/"boundariesDict"
            << exit(FatalError);
    }
 
	DynamicList<word> moleculesReduced(0);

    forAll(molecules, i)
    {
        const word& moleculeName(molecules[i]);

        if(findIndex(moleculesReduced, moleculeName) == -1)
        {
            moleculesReduced.append(moleculeName);
        }
    }

    moleculesReduced.shrink();

    //  set the type ids

    typeIds_.setSize(moleculesReduced.size(), -1);

    forAll(moleculesReduced, i)
    {
        const word& moleculeName(moleculesReduced[i]);

        label typeId(findIndex(cloud_.typeIdList(), moleculeName));

        if(typeId == -1)
        {
            FatalErrorIn("dsmcYePressureOutletCalculatedMolarFraction::dsmcYePressureOutletCalculatedMolarFraction()")
                << "Cannot find typeId: " << moleculeName << nl << "in: "
                << mesh_.time().system()/"boundariesDict"
                << exit(FatalError);
        }

        typeIds_[i] = typeId;
    }

    // set the accumulator  

    accumulatedParcelsToInsert_.setSize(typeIds_.size());

    forAll(accumulatedParcelsToInsert_, m)
    {
        accumulatedParcelsToInsert_[m].setSize(nFaces_, 0.0);
    }
    
	vibT_.setSize(typeIds_.size());

    forAll(vibT_, m)
    {
        vibT_[m].setSize(nFaces_, 0.0);
    }
    
	vDof_.setSize(typeIds_.size());

    forAll(vDof_, m)
    {
        vDof_[m].setSize(nFaces_, 0.0);
    }
   
	totalVibrationalEnergy_.setSize(typeIds_.size());

    forAll(totalVibrationalEnergy_, m)
    {
        totalVibrationalEnergy_[m].setSize(nFaces_, 0.0);
    }
    
	nTotalParcelsSpecies_.setSize(typeIds_.size());

    forAll(nTotalParcelsSpecies_, m)
    {
        nTotalParcelsSpecies_[m].setSize(nFaces_, 0.0);
    }
    
    moleFractions_.setSize(typeIds_.size());

    forAll(moleFractions_, m)
    {
        moleFractions_[m].setSize(nFaces_, 1.0/typeIds_.size());   //  At the first step it's assumed that all spc's have the same molar fraction. it is neccessary to start with non-zero mole fractions
    }
}


void dsmcYePressureOutletCalculatedMolarFraction::setNewBoundaryFields()
{

}



} // End namespace Foam

// ************************************************************************* //
