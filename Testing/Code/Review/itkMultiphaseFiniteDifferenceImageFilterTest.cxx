/*=========================================================================

  Program:   Insight Segmentation & Registration Toolkit
  Module:    itkMultiphaseFiniteDifferenceImageFilterTest.cxx
  Language:  C++
  Date:      $Date$
  Version:   $Revision$

  Copyright (c) Insight Software Consortium. All rights reserved.
  See ITKCopyright.txt or http://www.itk.org/HTML/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#if defined(_MSC_VER)
#pragma warning ( disable : 4786 )
#endif

#include "itkMultiphaseFiniteDifferenceImageFilter.h"
#include "itkScalarChanAndVeseLevelSetFunction.h"
#include "itkScalarChanAndVeseLevelSetFunctionSharedData.h"
#include "itkImage.h"
#include "itkTestingMacros.h"

namespace itk
{

template < class TInputImage, class TOutputImage,
  class TFiniteDifferenceFunction = FiniteDifferenceFunction<TOutputImage>,
  typename TIdCell = unsigned int >
class MultiphaseFiniteDifferenceImageFilterTestHelper
  : public MultiphaseFiniteDifferenceImageFilter< 
      TInputImage, TOutputImage, TFiniteDifferenceFunction, TIdCell >
{
public:
  /** Standard class typedefs. */
  typedef MultiphaseFiniteDifferenceImageFilterTestHelper           Self;
  typedef MultiphaseFiniteDifferenceImageFilter<
    TInputImage, TOutputImage,TFiniteDifferenceFunction,TIdCell>    Superclass;
  typedef SmartPointer<Self>                                        Pointer;
  typedef SmartPointer<const Self>                                  ConstPointer;

  /** Run-time type information (and related methods) */
  itkTypeMacro( MultiphaseFiniteDifferenceImageFilterTestHelper, MultiphaseFiniteDifferenceImageFilter );

  itkNewMacro( Self );

  virtual void AllocateUpdateBuffer() {}

  typedef typename Superclass::TimeStepType   TimeStepType;

  virtual void ApplyUpdate(TimeStepType itkNotUsed(dt) ) {}

  virtual TimeStepType CalculateChange()
    {
    return TimeStepType( 1.0 );
    }

  virtual void CopyInputToOutput() {}
 
};


}

int itkMultiphaseFiniteDifferenceImageFilterTest( int, char* [] )
{
  const unsigned int Dimension = 3;

  typedef double                                  PixelType;
  typedef itk::Image< PixelType, Dimension >      ImageType;
  typedef itk::Image< float, Dimension >          FeatureImageType;

  typedef itk::ScalarChanAndVeseLevelSetFunctionSharedData< 
    ImageType, FeatureImageType >      DataHelperType;


  typedef itk::ScalarChanAndVeseLevelSetFunction< 
    ImageType, FeatureImageType, DataHelperType >      RegionBasedLevelSetFunctionType;

  RegionBasedLevelSetFunctionType::Pointer function = RegionBasedLevelSetFunctionType::New();
 
  typedef itk::MultiphaseFiniteDifferenceImageFilterTestHelper< 
    ImageType, ImageType, RegionBasedLevelSetFunctionType >  FilterType;
 
  FilterType::Pointer filter = FilterType::New();

  std::cout << "GetNameOfClass() = " << filter->GetNameOfClass() << std::endl;
  filter->Print( std::cout );


  return EXIT_SUCCESS;
}
