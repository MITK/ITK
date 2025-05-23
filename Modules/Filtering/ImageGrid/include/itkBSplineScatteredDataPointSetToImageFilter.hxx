/*=========================================================================
 *
 *  Copyright NumFOCUS
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *         https://www.apache.org/licenses/LICENSE-2.0.txt
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *=========================================================================*/
#ifndef itkBSplineScatteredDataPointSetToImageFilter_hxx
#define itkBSplineScatteredDataPointSetToImageFilter_hxx

#include "itkImageRegionIterator.h"
#include "itkImageRegionIteratorWithIndex.h"
#include "itkImageDuplicator.h"
#include "itkCastImageFilter.h"
#include "itkNumericTraits.h"
#include "itkMath.h"
#include "itkPrintHelper.h"
#include "vnl/algo/vnl_matrix_inverse.h"

namespace itk
{

template <typename TInputPointSet, typename TOutputImage>
BSplineScatteredDataPointSetToImageFilter<TInputPointSet, TOutputImage>::BSplineScatteredDataPointSetToImageFilter()

{
  this->m_SplineOrder.Fill(3);
  this->DynamicMultiThreadingOff();

  for (unsigned int i = 0; i < ImageDimension; ++i)
  {
    this->m_NumberOfControlPoints[i] = this->m_SplineOrder[i] + 1;
    this->m_Kernel[i] = KernelType::New();
    this->m_Kernel[i]->SetSplineOrder(this->m_SplineOrder[i]);
  }

  this->m_CurrentNumberOfControlPoints = this->m_NumberOfControlPoints;
  this->m_KernelOrder0 = KernelOrder0Type::New();
  this->m_KernelOrder1 = KernelOrder1Type::New();
  this->m_KernelOrder2 = KernelOrder2Type::New();
  this->m_KernelOrder3 = KernelOrder3Type::New();

  this->m_CloseDimension.Fill(0);

  this->m_NumberOfLevels.Fill(1);

  this->m_PsiLattice = PointDataImageType::New();

  for (unsigned int i = 0; i < ImageDimension; ++i)
  {
    this->m_RefinedLatticeCoefficients[i].fill(0.0);
  }

  this->m_ResidualPointSetValues = PointDataContainerType::New();

  this->m_PointWeights = WeightsContainerType::New();
}

template <typename TInputPointSet, typename TOutputImage>
void
BSplineScatteredDataPointSetToImageFilter<TInputPointSet, TOutputImage>::SetSplineOrder(unsigned int order)
{
  this->m_SplineOrder.Fill(order);
  this->SetSplineOrder(this->m_SplineOrder);
}

template <typename TInputPointSet, typename TOutputImage>
void
BSplineScatteredDataPointSetToImageFilter<TInputPointSet, TOutputImage>::SetSplineOrder(const ArrayType & order)
{
  itkDebugMacro("Setting m_SplineOrder to " << order);

  this->m_SplineOrder = order;
  for (unsigned int i = 0; i < ImageDimension; ++i)
  {
    if (this->m_SplineOrder[i] == 0)
    {
      itkExceptionMacro("The spline order in each dimension must be greater than 0");
    }

    this->m_Kernel[i] = KernelType::New();
    this->m_Kernel[i]->SetSplineOrder(this->m_SplineOrder[i]);

    if (this->m_DoMultilevel)
    {
      typename KernelType::MatrixType C;
      C = this->m_Kernel[i]->GetShapeFunctionsInZeroToOneInterval();

      vnl_matrix<RealType> R;
      vnl_matrix<RealType> S;
      R.set_size(C.rows(), C.cols());
      S.set_size(C.rows(), C.cols());
      for (unsigned int j = 0; j < C.rows(); ++j)
      {
        for (unsigned int k = 0; k < C.cols(); ++k)
        {
          R(j, k) = S(j, k) = static_cast<RealType>(C(j, k));
        }
      }
      for (unsigned int j = 0; j < C.cols(); ++j)
      {
        RealType c = std::pow(static_cast<RealType>(2.0), static_cast<RealType>(C.cols()) - j - 1);

        for (unsigned int k = 0; k < C.rows(); ++k)
        {
          R(k, j) *= c;
        }
      }
      R = R.transpose();
      R.flipud();
      S = S.transpose();
      S.flipud();

      this->m_RefinedLatticeCoefficients[i] = (vnl_svd<RealType>(R).solve(S)).extract(2, S.cols());
    }
  }
  this->Modified();
}

template <typename TInputPointSet, typename TOutputImage>
void
BSplineScatteredDataPointSetToImageFilter<TInputPointSet, TOutputImage>::SetNumberOfLevels(unsigned int levels)
{
  this->m_NumberOfLevels.Fill(levels);
  this->SetNumberOfLevels(this->m_NumberOfLevels);
}

template <typename TInputPointSet, typename TOutputImage>
void
BSplineScatteredDataPointSetToImageFilter<TInputPointSet, TOutputImage>::SetNumberOfLevels(const ArrayType & levels)
{
  this->m_NumberOfLevels = levels;
  this->m_MaximumNumberOfLevels = 1;
  for (unsigned int i = 0; i < ImageDimension; ++i)
  {
    if (this->m_NumberOfLevels[i] == 0)
    {
      itkExceptionMacro("The number of levels in each dimension must be greater than 0");
    }
    if (this->m_NumberOfLevels[i] > this->m_MaximumNumberOfLevels)
    {
      this->m_MaximumNumberOfLevels = this->m_NumberOfLevels[i];
    }
  }

  itkDebugMacro("Setting m_NumberOfLevels to " << this->m_NumberOfLevels);
  itkDebugMacro("Setting m_MaximumNumberOfLevels to " << this->m_MaximumNumberOfLevels);

  if (this->m_MaximumNumberOfLevels > 1)
  {
    this->m_DoMultilevel = true;
  }
  else
  {
    this->m_DoMultilevel = false;
  }
  this->SetSplineOrder(this->m_SplineOrder);
  this->Modified();
}

template <typename TInputPointSet, typename TOutputImage>
void
BSplineScatteredDataPointSetToImageFilter<TInputPointSet, TOutputImage>::SetPointWeights(WeightsContainerType * weights)
{
  this->m_UsePointWeights = true;
  this->m_PointWeights = weights;
  this->Modified();
}

template <typename TInputPointSet, typename TOutputImage>
void
BSplineScatteredDataPointSetToImageFilter<TInputPointSet, TOutputImage>::GenerateData()
{
  TOutputImage *         output = this->GetOutput();
  const TInputPointSet * inputPointSet = this->GetInput();

  // Create the output image

  itkDebugMacro("Size: " << this->m_Size);
  itkDebugMacro("Origin: " << this->m_Origin);
  itkDebugMacro("Spacing: " << this->m_Spacing);
  itkDebugMacro("Direction: " << this->m_Direction);

  for (unsigned int i = 0; i < ImageDimension; ++i)
  {
    if (this->m_Size[i] == 0)
    {
      itkExceptionMacro("Size must be specified.");
    }
  }

  output->SetOrigin(this->m_Origin);
  output->SetSpacing(this->m_Spacing);
  output->SetDirection(this->m_Direction);
  output->SetRegions(this->m_Size);
  output->Allocate();


  // Perform some error checking on the input

  if (this->m_UsePointWeights && (this->m_PointWeights->Size() != inputPointSet->GetNumberOfPoints()))
  {
    itkExceptionMacro("The number of weight points and input points must be equal.");
  }

  for (unsigned int i = 0; i < ImageDimension; ++i)
  {
    if (this->m_NumberOfControlPoints[i] < this->m_SplineOrder[i] + 1)
    {
      itkExceptionMacro("The number of control points must be greater than the spline order.");
    }
  }

  unsigned int maximumNumberOfSpans = 0;
  for (unsigned int d = 0; d < ImageDimension; ++d)
  {
    unsigned int numberOfSpans = this->m_NumberOfControlPoints[d] - this->m_SplineOrder[d];
    numberOfSpans <<= (this->m_NumberOfLevels[d] - 1);
    if (numberOfSpans > maximumNumberOfSpans)
    {
      maximumNumberOfSpans = numberOfSpans;
    }
  }

  this->m_ResidualPointSetValues->Initialize();
  if (inputPointSet->GetNumberOfPoints() > 0)
  {
    const auto & pointData = inputPointSet->GetPointData()->CastToSTLConstContainer();
    if (!this->m_UsePointWeights)
    {
      this->m_PointWeights->CastToSTLContainer().assign(pointData.size(), 1);
    }
    // Use the residuals to compute the higher level solutions but start with the original
    // values for the first level.
    this->m_ResidualPointSetValues->CastToSTLContainer().assign(pointData.begin(), pointData.end());
  }

  this->m_CurrentLevel = 0;
  this->m_CurrentNumberOfControlPoints = this->m_NumberOfControlPoints;

  // Set up multithread processing
  typename ImageSource<TOutputImage>::ThreadStruct str1;
  str1.Filter = this;

  MultiThreaderBase * multiThreader = this->GetMultiThreader();
  multiThreader->SetNumberOfWorkUnits(this->GetNumberOfWorkUnits());
  multiThreader->SetSingleMethod(this->ThreaderCallback, &str1);

  // Multithread the generation of the control point lattice for the first level.
  this->BeforeThreadedGenerateData();
  multiThreader->SingleMethodExecute();
  this->AfterThreadedGenerateData();

  if (this->m_DoMultilevel)
  {
    this->m_PsiLattice->SetRegions(this->m_PhiLattice->GetLargestPossibleRegion());
    this->m_PsiLattice->Allocate();
    PointDataType P{};
    this->m_PsiLattice->FillBuffer(P);

    for (this->m_CurrentLevel = 1; this->m_CurrentLevel < this->m_MaximumNumberOfLevels; this->m_CurrentLevel++)
    {
      // Multithread updating the point set values
      this->m_DoUpdateResidualValues = true;
      // this->BeforeThreadedGenerateData();
      multiThreader->SingleMethodExecute();
      // this->AfterThreadedGenerateData();
      this->m_DoUpdateResidualValues = false;

      ImageRegionIterator<PointDataImageType> ItPsi(this->m_PsiLattice, this->m_PsiLattice->GetLargestPossibleRegion());
      ImageRegionIterator<PointDataImageType> ItPhi(this->m_PhiLattice, this->m_PhiLattice->GetLargestPossibleRegion());
      for (ItPsi.GoToBegin(), ItPhi.GoToBegin(); !ItPsi.IsAtEnd(); ++ItPsi, ++ItPhi)
      {
        ItPsi.Set(ItPhi.Get() + ItPsi.Get());
      }
      this->RefineControlPointLattice();

      for (unsigned int i = 0; i < ImageDimension; ++i)
      {
        if (this->m_CurrentLevel < this->m_NumberOfLevels[i])
        {
          this->m_CurrentNumberOfControlPoints[i] =
            2 * this->m_CurrentNumberOfControlPoints[i] - this->m_SplineOrder[i];
        }
      }

      itkDebugMacro("Current Level = " << this->m_CurrentLevel);
      itkDebugMacro("  Current number of control points = " << this->m_CurrentNumberOfControlPoints);

      // Multithread the generation of the control point lattice.
      this->BeforeThreadedGenerateData();
      multiThreader->SingleMethodExecute();
      this->AfterThreadedGenerateData();
    }

    ImageRegionIterator<PointDataImageType> ItPsi(this->m_PsiLattice, this->m_PsiLattice->GetLargestPossibleRegion());
    ImageRegionIterator<PointDataImageType> ItPhi(this->m_PhiLattice, this->m_PhiLattice->GetLargestPossibleRegion());
    for (ItPsi.GoToBegin(), ItPhi.GoToBegin(); !ItPsi.IsAtEnd(); ++ItPsi, ++ItPhi)
    {
      ItPsi.Set(ItPhi.Get() + ItPsi.Get());
    }

    using ImageDuplicatorType = ImageDuplicator<PointDataImageType>;
    auto duplicator = ImageDuplicatorType::New();
    duplicator->SetInputImage(this->m_PsiLattice);
    duplicator->Update();
    this->m_PhiLattice = duplicator->GetOutput();
  }

  this->m_IsFittingComplete = true;

  // Multithread the reconstruction of the sampled B-spline object
  if (this->m_GenerateOutputImage)
  {
    //    this->BeforeThreadedGenerateData();
    multiThreader->SingleMethodExecute();
    //    this->AfterThreadedGenerateData();
  }

  this->SetPhiLatticeParametricDomainParameters();
}

template <typename TInputPointSet, typename TOutputImage>
void
BSplineScatteredDataPointSetToImageFilter<TInputPointSet, TOutputImage>::BeforeThreadedGenerateData()
{
  if (!this->m_IsFittingComplete)
  {
    this->m_DeltaLatticePerThread.resize(this->GetNumberOfWorkUnits());
    this->m_OmegaLatticePerThread.resize(this->GetNumberOfWorkUnits());

    typename RealImageType::SizeType size;
    for (unsigned int i = 0; i < ImageDimension; ++i)
    {
      if (this->m_CloseDimension[i])
      {
        size[i] = this->m_CurrentNumberOfControlPoints[i] - this->m_SplineOrder[i];
      }
      else
      {
        size[i] = this->m_CurrentNumberOfControlPoints[i];
      }
    }

    for (unsigned int n = 0; n < this->GetNumberOfWorkUnits(); ++n)
    {
      this->m_OmegaLatticePerThread[n] = RealImageType::New();
      this->m_OmegaLatticePerThread[n]->SetRegions(size);
      this->m_OmegaLatticePerThread[n]->AllocateInitialized();

      this->m_DeltaLatticePerThread[n] = PointDataImageType::New();
      this->m_DeltaLatticePerThread[n]->SetRegions(size);
      this->m_DeltaLatticePerThread[n]->AllocateInitialized();
    }
  }
}

template <typename TInputPointSet, typename TOutputImage>
unsigned int
BSplineScatteredDataPointSetToImageFilter<TInputPointSet, TOutputImage>::SplitRequestedRegion(unsigned int i,
                                                                                              unsigned int num,
                                                                                              RegionType & splitRegion)
{
  // For fitting, the image regions are not used so we always return a valid
  // number.
  if (!this->m_IsFittingComplete)
  {
    return this->GetNumberOfWorkUnits();
  }
  else // we split on the output region for reconstruction
  {
    return Superclass::SplitRequestedRegion(i, num, splitRegion);
  }
}

template <typename TInputPointSet, typename TOutputImage>
void
BSplineScatteredDataPointSetToImageFilter<TInputPointSet, TOutputImage>::ThreadedGenerateData(const RegionType & region,
                                                                                              ThreadIdType threadId)
{
  if (!this->m_IsFittingComplete)
  {
    if (this->m_DoUpdateResidualValues)
    {
      this->ThreadedGenerateDataForUpdatingResidualValues(region, threadId);
    }
    else
    {
      this->ThreadedGenerateDataForFitting(region, threadId);
    }
  }
  else
  {
    this->ThreadedGenerateDataForReconstruction(region, threadId);
  }
}

template <typename TInputPointSet, typename TOutputImage>
void
BSplineScatteredDataPointSetToImageFilter<TInputPointSet, TOutputImage>::ThreadedGenerateDataForFitting(
  const RegionType & itkNotUsed(region),
  ThreadIdType       threadId)
{
  const TInputPointSet * input = this->GetInput();

  // Ignore the output region as we're only interested in dividing the
  // points among the threads.

  typename RealImageType::SizeType size;

  for (unsigned int i = 0; i < ImageDimension; ++i)
  {
    size[i] = this->m_SplineOrder[i] + 1;
  }
  RealImagePointer neighborhoodWeightImage = RealImageType::New();
  neighborhoodWeightImage->SetRegions(size);
  neighborhoodWeightImage->AllocateInitialized();

  ImageRegionIteratorWithIndex<RealImageType> ItW(neighborhoodWeightImage,
                                                  neighborhoodWeightImage->GetRequestedRegion());

  RealArrayType p;
  RealArrayType r;
  RealArrayType epsilon;
  for (unsigned int i = 0; i < ImageDimension; ++i)
  {
    r[i] = static_cast<RealType>(this->m_CurrentNumberOfControlPoints[i] - this->m_SplineOrder[i]) /
           (static_cast<RealType>(this->m_Size[i] - 1) * this->m_Spacing[i]);
    epsilon[i] = r[i] * this->m_Spacing[i] * this->m_BSplineEpsilon;
  }

  // Determine which points should be handled by this particular thread.

  ThreadIdType numberOfWorkUnits = this->GetNumberOfWorkUnits();
  auto         numberOfPointsPerThread = static_cast<SizeValueType>(input->GetNumberOfPoints() / numberOfWorkUnits);

  unsigned int start = threadId * numberOfPointsPerThread;
  unsigned int end = start + numberOfPointsPerThread;
  if (threadId == this->GetNumberOfWorkUnits() - 1)
  {
    end = input->GetNumberOfPoints();
  }

  for (unsigned int n = start; n < end; ++n)
  {
    PointType point;
    point.Fill(0.0);

    input->GetPoint(n, &point);

    for (unsigned int i = 0; i < ImageDimension; ++i)
    {
      unsigned int totalNumberOfSpans = this->m_CurrentNumberOfControlPoints[i] - this->m_SplineOrder[i];

      p[i] = (point[i] - this->m_Origin[i]) * r[i];
      if (itk::Math::abs(p[i] - static_cast<RealType>(totalNumberOfSpans)) <= epsilon[i])
      {
        p[i] = static_cast<RealType>(totalNumberOfSpans) - epsilon[i];
      }
      if (p[i] < RealType{} && itk::Math::abs(p[i]) <= epsilon[i])
      {
        p[i] = RealType{};
      }

      if (p[i] < RealType{} || p[i] >= static_cast<RealType>(totalNumberOfSpans))
      {
        itkExceptionMacro("The reparameterized point component "
                          << p[i] << " is outside the corresponding parametric domain of [0, " << totalNumberOfSpans
                          << ").");
      }
    }

    RealType w2Sum = 0.0;
    for (ItW.GoToBegin(); !ItW.IsAtEnd(); ++ItW)
    {
      RealType                          B = 1.0;
      typename RealImageType::IndexType idx = ItW.GetIndex();
      for (unsigned int i = 0; i < ImageDimension; ++i)
      {
        RealType u = static_cast<RealType>(p[i] - static_cast<unsigned int>(p[i]) - idx[i]) +
                     0.5 * static_cast<RealType>(this->m_SplineOrder[i] - 1);

        switch (this->m_SplineOrder[i])
        {
          case 0:
          {
            B *= this->m_KernelOrder0->Evaluate(u);
            break;
          }
          case 1:
          {
            B *= this->m_KernelOrder1->Evaluate(u);
            break;
          }
          case 2:
          {
            B *= this->m_KernelOrder2->Evaluate(u);
            break;
          }
          case 3:
          {
            B *= this->m_KernelOrder3->Evaluate(u);
            break;
          }
          default:
          {
            B *= this->m_Kernel[i]->Evaluate(u);
            break;
          }
        }
      }
      ItW.Set(B);
      w2Sum += B * B;
    }

    RealImageType *      currentThreadOmegaLattice = this->m_OmegaLatticePerThread[threadId];
    PointDataImageType * currentThreadDeltaLattice = this->m_DeltaLatticePerThread[threadId];

    for (ItW.GoToBegin(); !ItW.IsAtEnd(); ++ItW)
    {
      typename RealImageType::IndexType idx = ItW.GetIndex();
      for (unsigned int i = 0; i < ImageDimension; ++i)
      {
        idx[i] += static_cast<unsigned int>(p[i]);
        if (this->m_CloseDimension[i])
        {
          idx[i] %= currentThreadDeltaLattice->GetLargestPossibleRegion().GetSize()[i];
        }
      }
      RealType wc = this->m_PointWeights->GetElement(n);
      RealType t = ItW.Get();
      currentThreadOmegaLattice->SetPixel(idx, currentThreadOmegaLattice->GetPixel(idx) + wc * t * t);
      PointDataType data = this->m_ResidualPointSetValues->GetElement(n);
      data *= (t * t * t * wc / w2Sum);
      currentThreadDeltaLattice->SetPixel(idx, currentThreadDeltaLattice->GetPixel(idx) + data);
    }
  }
}

template <typename TInputPointSet, typename TOutputImage>
void
BSplineScatteredDataPointSetToImageFilter<TInputPointSet, TOutputImage>::ThreadedGenerateDataForReconstruction(
  const RegionType & region,
  ThreadIdType       itkNotUsed(threadId))
{
  typename PointDataImageType::Pointer collapsedPhiLattices[ImageDimension + 1];
  for (unsigned int i = 0; i < ImageDimension; ++i)
  {
    collapsedPhiLattices[i] = PointDataImageType::New();
    collapsedPhiLattices[i]->CopyInformation(this->m_PhiLattice);

    typename PointDataImageType::SizeType size;
    size.Fill(1);
    for (unsigned int j = 0; j < i; ++j)
    {
      size[j] = this->m_PhiLattice->GetLargestPossibleRegion().GetSize()[j];
    }
    collapsedPhiLattices[i]->SetRegions(size);
    collapsedPhiLattices[i]->Allocate();
  }
  using ImageDuplicatorType = ImageDuplicator<PointDataImageType>;
  auto duplicator = ImageDuplicatorType::New();
  duplicator->SetInputImage(this->m_PhiLattice);
  duplicator->Update();

  collapsedPhiLattices[ImageDimension] = duplicator->GetOutput();

  ArrayType totalNumberOfSpans;
  for (unsigned int i = 0; i < ImageDimension; ++i)
  {
    if (this->m_CloseDimension[i])
    {
      totalNumberOfSpans[i] = this->m_PhiLattice->GetLargestPossibleRegion().GetSize()[i];
    }
    else
    {
      totalNumberOfSpans[i] = this->m_PhiLattice->GetLargestPossibleRegion().GetSize()[i] - this->m_SplineOrder[i];
    }
  }

  RealArrayType r;
  for (unsigned int i = 0; i < ImageDimension; ++i)
  {
    r[i] =
      static_cast<RealType>(totalNumberOfSpans[i]) / (static_cast<RealType>(this->m_Size[i] - 1) * this->m_Spacing[i]);
  }

  RealArrayType epsilon;
  for (unsigned int i = 0; i < ImageDimension; ++i)
  {
    epsilon[i] = r[i] * this->m_Spacing[i] * this->m_BSplineEpsilon;
  }

  FixedArray<RealType, ImageDimension> U;
  FixedArray<RealType, ImageDimension> currentU;
  currentU.Fill(-1);

  typename ImageType::IndexType          startIndex = this->GetOutput()->GetRequestedRegion().GetIndex();
  typename PointDataImageType::IndexType startPhiIndex = this->m_PhiLattice->GetLargestPossibleRegion().GetIndex();

  for (ImageRegionIteratorWithIndex<ImageType> It(this->GetOutput(), region); !It.IsAtEnd(); ++It)
  {
    typename ImageType::IndexType idx = It.GetIndex();
    for (unsigned int i = 0; i < ImageDimension; ++i)
    {
      U[i] = static_cast<RealType>(totalNumberOfSpans[i]) * static_cast<RealType>(idx[i] - startIndex[i]) /
             static_cast<RealType>(this->m_Size[i] - 1);

      if (itk::Math::abs(U[i] - static_cast<RealType>(totalNumberOfSpans[i])) <= epsilon[i])
      {
        U[i] = static_cast<RealType>(totalNumberOfSpans[i]) - epsilon[i];
      }
      if (U[i] < RealType{} && itk::Math::abs(U[i]) <= epsilon[i])
      {
        U[i] = RealType{};
      }

      if (U[i] < RealType{} || U[i] >= static_cast<RealType>(totalNumberOfSpans[i]))
      {
        itkExceptionMacro("The collapse point component "
                          << U[i] << " is outside the corresponding parametric domain of [0, " << totalNumberOfSpans[i]
                          << ").");
      }
    }
    for (int i = ImageDimension - 1; i >= 0; i--)
    {
      if (Math::NotExactlyEquals(U[i], currentU[i]))
      {
        for (int j = i; j >= 0; j--)
        {
          this->CollapsePhiLattice(collapsedPhiLattices[j + 1], collapsedPhiLattices[j], U[j], j);
          currentU[j] = U[j];
        }
        break;
      }
    }
    It.Set(collapsedPhiLattices[0]->GetPixel(startPhiIndex));
  }
}

template <typename TInputPointSet, typename TOutputImage>
void
BSplineScatteredDataPointSetToImageFilter<TInputPointSet, TOutputImage>::AfterThreadedGenerateData()
{
  if (!this->m_IsFittingComplete)
  {
    // Accumulate all the delta lattice and omega lattice values to
    // calculate the final phi lattice.

    ImageRegionIterator<PointDataImageType> ItD(this->m_DeltaLatticePerThread[0],
                                                this->m_DeltaLatticePerThread[0]->GetLargestPossibleRegion());
    ImageRegionIterator<RealImageType>      ItO(this->m_OmegaLatticePerThread[0],
                                           this->m_OmegaLatticePerThread[0]->GetLargestPossibleRegion());

    for (ThreadIdType n = 1; n < this->GetNumberOfWorkUnits(); ++n)
    {
      ImageRegionIterator<PointDataImageType> Itd(this->m_DeltaLatticePerThread[n],
                                                  this->m_DeltaLatticePerThread[n]->GetLargestPossibleRegion());
      ImageRegionIterator<RealImageType>      Ito(this->m_OmegaLatticePerThread[n],
                                             this->m_OmegaLatticePerThread[n]->GetLargestPossibleRegion());

      ItD.GoToBegin();
      ItO.GoToBegin();
      Itd.GoToBegin();
      Ito.GoToBegin();
      while (!ItD.IsAtEnd())
      {
        ItD.Set(ItD.Get() + Itd.Get());
        ItO.Set(ItO.Get() + Ito.Get());

        ++ItD;
        ++ItO;
        ++Itd;
        ++Ito;
      }
    }

    // Generate the control point lattice

    typename RealImageType::SizeType size;
    for (unsigned int i = 0; i < ImageDimension; ++i)
    {
      if (this->m_CloseDimension[i])
      {
        size[i] = this->m_CurrentNumberOfControlPoints[i] - this->m_SplineOrder[i];
      }
      else
      {
        size[i] = this->m_CurrentNumberOfControlPoints[i];
      }
    }
    this->m_PhiLattice = PointDataImageType::New();
    this->m_PhiLattice->SetRegions(size);
    this->m_PhiLattice->AllocateInitialized();

    ImageRegionIterator<PointDataImageType> ItP(this->m_PhiLattice, this->m_PhiLattice->GetLargestPossibleRegion());

    for (ItP.GoToBegin(), ItO.GoToBegin(), ItD.GoToBegin(); !ItP.IsAtEnd(); ++ItP, ++ItO, ++ItD)
    {
      PointDataType P;
      P.Fill(0);
      if (Math::NotAlmostEquals(ItO.Get(), typename PointDataType::ValueType{}))
      {
        P = ItD.Get() / ItO.Get();
        for (unsigned int i = 0; i < P.Size(); ++i)
        {
          if (itk::Math::isnan(P[i]) || itk::Math::isinf(P[i]))
          {
            P[i] = 0;
          }
        }
        ItP.Set(P);
      }
    }
  }
}

template <typename TInputPointSet, typename TOutputImage>
void
BSplineScatteredDataPointSetToImageFilter<TInputPointSet, TOutputImage>::RefineControlPointLattice()
{
  ArrayType NumberOfNewControlPoints = this->m_CurrentNumberOfControlPoints;

  for (unsigned int i = 0; i < ImageDimension; ++i)
  {
    if (this->m_CurrentLevel < this->m_NumberOfLevels[i])
    {
      NumberOfNewControlPoints[i] = 2 * NumberOfNewControlPoints[i] - this->m_SplineOrder[i];
    }
  }
  typename RealImageType::SizeType size;
  for (unsigned int i = 0; i < ImageDimension; ++i)
  {
    if (this->m_CloseDimension[i])
    {
      size[i] = NumberOfNewControlPoints[i] - this->m_SplineOrder[i];
    }
    else
    {
      size[i] = NumberOfNewControlPoints[i];
    }
  }

  PointDataImagePointer refinedLattice = PointDataImageType::New();
  refinedLattice->SetRegions(size);
  refinedLattice->Allocate();

  PointDataType data;
  data.Fill(0.0);
  refinedLattice->FillBuffer(data);

  typename PointDataImageType::IndexType            idx;
  typename PointDataImageType::IndexType            idxPsi;
  typename PointDataImageType::IndexType            tmp;
  typename PointDataImageType::IndexType            tmpPsi;
  typename PointDataImageType::IndexType            off;
  typename PointDataImageType::IndexType            offPsi;
  typename PointDataImageType::RegionType::SizeType sizePsi;

  size.Fill(2);
  unsigned int N = 1;
  for (unsigned int i = 0; i < ImageDimension; ++i)
  {
    N *= (this->m_SplineOrder[i] + 1);
    sizePsi[i] = this->m_SplineOrder[i] + 1;
  }

  ImageRegionIteratorWithIndex<PointDataImageType> It(refinedLattice, refinedLattice->GetLargestPossibleRegion());

  while (!It.IsAtEnd())
  {
    idx = It.GetIndex();
    for (unsigned int i = 0; i < ImageDimension; ++i)
    {
      if (this->m_CurrentLevel < this->m_NumberOfLevels[i])
      {
        idxPsi[i] = static_cast<unsigned int>(0.5 * idx[i]);
      }
      else
      {
        idxPsi[i] = static_cast<unsigned int>(idx[i]);
      }
    }

    for (unsigned int i = 0; i < (2 << (ImageDimension - 1)); ++i)
    {
      PointDataType sum{};
      PointDataType val{};
      off = this->NumberToIndex(i, size);

      bool outOfBoundary = false;
      for (unsigned int j = 0; j < ImageDimension; ++j)
      {
        tmp[j] = idx[j] + off[j];
        if (tmp[j] >= static_cast<int>(NumberOfNewControlPoints[j]) && !this->m_CloseDimension[j])
        {
          outOfBoundary = true;
          break;
        }
        if (this->m_CloseDimension[j])
        {
          tmp[j] %= refinedLattice->GetLargestPossibleRegion().GetSize()[j];
        }
      }
      if (outOfBoundary)
      {
        continue;
      }

      for (unsigned int j = 0; j < N; ++j)
      {
        offPsi = this->NumberToIndex(j, sizePsi);

        bool isOutOfBoundary = false;
        for (unsigned int k = 0; k < ImageDimension; ++k)
        {
          tmpPsi[k] = idxPsi[k] + offPsi[k];
          if (tmpPsi[k] >= static_cast<int>(this->m_CurrentNumberOfControlPoints[k]) && !this->m_CloseDimension[k])
          {
            isOutOfBoundary = true;
            break;
          }
          if (this->m_CloseDimension[k])
          {
            tmpPsi[k] %= this->m_PsiLattice->GetLargestPossibleRegion().GetSize()[k];
          }
        }
        if (isOutOfBoundary)
        {
          continue;
        }
        RealType coeff = 1.0;
        for (unsigned int k = 0; k < ImageDimension; ++k)
        {
          coeff *= this->m_RefinedLatticeCoefficients[k](off[k], offPsi[k]);
        }
        val = this->m_PsiLattice->GetPixel(tmpPsi);
        val *= coeff;
        sum += val;
      }
      refinedLattice->SetPixel(tmp, sum);
    }

    bool isEvenIndex = false;
    while (!isEvenIndex && !It.IsAtEnd())
    {
      ++It;
      idx = It.GetIndex();
      isEvenIndex = true;
      for (unsigned int i = 0; i < ImageDimension; ++i)
      {
        if (idx[i] % 2)
        {
          isEvenIndex = false;
        }
      }
    }
  }

  using ImageDuplicatorType = ImageDuplicator<PointDataImageType>;
  auto duplicator = ImageDuplicatorType::New();
  duplicator->SetInputImage(refinedLattice);
  duplicator->Update();
  this->m_PsiLattice = duplicator->GetOutput();
}

template <typename TInputPointSet, typename TOutputImage>
void
BSplineScatteredDataPointSetToImageFilter<TInputPointSet, TOutputImage>::ThreadedGenerateDataForUpdatingResidualValues(
  const RegionType & itkNotUsed(region),
  ThreadIdType       threadId)
{
  const TInputPointSet * input = this->GetInput();
  PointDataImagePointer  collapsedPhiLattices[ImageDimension + 1];
  for (unsigned int i = 0; i < ImageDimension; ++i)
  {
    collapsedPhiLattices[i] = PointDataImageType::New();
    collapsedPhiLattices[i]->SetOrigin(this->m_PhiLattice->GetOrigin());
    collapsedPhiLattices[i]->SetSpacing(this->m_PhiLattice->GetSpacing());
    collapsedPhiLattices[i]->SetDirection(this->m_PhiLattice->GetDirection());

    typename PointDataImageType::SizeType size;
    size.Fill(1);
    for (unsigned int j = 0; j < i; ++j)
    {
      size[j] = this->m_PhiLattice->GetLargestPossibleRegion().GetSize()[j];
    }
    collapsedPhiLattices[i]->SetRegions(size);
    collapsedPhiLattices[i]->Allocate();
  }
  collapsedPhiLattices[ImageDimension] = this->m_PhiLattice;
  ArrayType totalNumberOfSpans;
  for (unsigned int i = 0; i < ImageDimension; ++i)
  {
    if (this->m_CloseDimension[i])
    {
      totalNumberOfSpans[i] = this->m_PhiLattice->GetLargestPossibleRegion().GetSize()[i];
    }
    else
    {
      totalNumberOfSpans[i] = this->m_PhiLattice->GetLargestPossibleRegion().GetSize()[i] - this->m_SplineOrder[i];
    }
  }

  RealArrayType r;
  for (unsigned int i = 0; i < ImageDimension; ++i)
  {
    r[i] =
      static_cast<RealType>(totalNumberOfSpans[i]) / (static_cast<RealType>(this->m_Size[i] - 1) * this->m_Spacing[i]);
  }

  RealArrayType epsilon;
  for (unsigned int i = 0; i < ImageDimension; ++i)
  {
    epsilon[i] = r[i] * this->m_Spacing[i] * this->m_BSplineEpsilon;
  }

  FixedArray<RealType, ImageDimension> U;
  FixedArray<RealType, ImageDimension> currentU;
  currentU.Fill(-1);

  typename PointDataImageType::IndexType startPhiIndex = this->m_PhiLattice->GetLargestPossibleRegion().GetIndex();

  // Determine which points should be handled by this particular thread.

  ThreadIdType numberOfWorkUnits = this->GetNumberOfWorkUnits();
  auto         numberOfPointsPerThread = static_cast<SizeValueType>(input->GetNumberOfPoints() / numberOfWorkUnits);

  unsigned int start = threadId * numberOfPointsPerThread;
  unsigned int end = start + numberOfPointsPerThread;
  if (threadId == this->GetNumberOfWorkUnits() - 1)
  {
    end = input->GetNumberOfPoints();
  }

  for (unsigned int n = start; n < end; ++n)
  {
    PointType point;
    point.Fill(0.0);

    input->GetPoint(n, &point);

    for (unsigned int i = 0; i < ImageDimension; ++i)
    {
      U[i] = static_cast<RealType>(totalNumberOfSpans[i]) * static_cast<RealType>(point[i] - this->m_Origin[i]) /
             (static_cast<RealType>(this->m_Size[i] - 1) * this->m_Spacing[i]);

      if (itk::Math::abs(U[i] - static_cast<RealType>(totalNumberOfSpans[i])) <= epsilon[i])
      {
        U[i] = static_cast<RealType>(totalNumberOfSpans[i]) - epsilon[i];
      }
      if (U[i] < RealType{} && itk::Math::abs(U[i]) <= epsilon[i])
      {
        U[i] = RealType{};
      }

      if (U[i] < RealType{} || U[i] >= static_cast<RealType>(totalNumberOfSpans[i]))
      {
        itkExceptionMacro("The collapse point component "
                          << U[i] << " is outside the corresponding parametric domain of [0, " << totalNumberOfSpans[i]
                          << ").");
      }
    }
    for (int i = ImageDimension - 1; i >= 0; i--)
    {
      if (Math::NotExactlyEquals(U[i], currentU[i]))
      {
        for (int j = i; j >= 0; j--)
        {
          this->CollapsePhiLattice(collapsedPhiLattices[j + 1], collapsedPhiLattices[j], U[j], j);
          currentU[j] = U[j];
        }
        break;
      }
    }
    this->m_ResidualPointSetValues->CastToSTLContainer()[n] -= collapsedPhiLattices[0]->GetPixel(startPhiIndex);
  }
}


template <typename TInputPointSet, typename TOutputImage>
void
BSplineScatteredDataPointSetToImageFilter<TInputPointSet, TOutputImage>::CollapsePhiLattice(
  PointDataImageType * lattice,
  PointDataImageType * collapsedLattice,
  const RealType       u,
  const unsigned int   dimension)
{
  for (ImageRegionIteratorWithIndex<PointDataImageType> It(collapsedLattice,
                                                           collapsedLattice->GetLargestPossibleRegion());
       !It.IsAtEnd();
       ++It)
  {
    PointDataType data;
    data.Fill(0.0);
    typename PointDataImageType::IndexType idx = It.GetIndex();
    for (unsigned int i = 0; i < this->m_SplineOrder[dimension] + 1; ++i)
    {
      idx[dimension] = static_cast<unsigned int>(u) + i;
      RealType v = u - idx[dimension] + 0.5 * static_cast<RealType>(this->m_SplineOrder[dimension] - 1);

      RealType B = 0.0;
      switch (this->m_SplineOrder[dimension])
      {
        case 0:
        {
          B = this->m_KernelOrder0->Evaluate(v);
          break;
        }
        case 1:
        {
          B = this->m_KernelOrder1->Evaluate(v);
          break;
        }
        case 2:
        {
          B = this->m_KernelOrder2->Evaluate(v);
          break;
        }
        case 3:
        {
          B = this->m_KernelOrder3->Evaluate(v);
          break;
        }
        default:
        {
          B = this->m_Kernel[dimension]->Evaluate(v);
          break;
        }
      }
      if (this->m_CloseDimension[dimension])
      {
        idx[dimension] %= lattice->GetLargestPossibleRegion().GetSize()[dimension];
      }
      data += (lattice->GetPixel(idx) * B);
    }
    It.Set(data);
  }
}

template <typename TInputPointSet, typename TOutputImage>
void
BSplineScatteredDataPointSetToImageFilter<TInputPointSet, TOutputImage>::SetPhiLatticeParametricDomainParameters()
{
  typename PointDataImageType::PointType   origin;
  typename PointDataImageType::SpacingType spacing;

  for (unsigned int i = 0; i < ImageDimension; ++i)
  {
    RealType domain = this->m_Spacing[i] * static_cast<RealType>(this->m_Size[i] - 1);

    unsigned int totalNumberOfSpans = this->m_PhiLattice->GetLargestPossibleRegion().GetSize()[i];
    if (!this->m_CloseDimension[i])
    {
      totalNumberOfSpans -= this->m_SplineOrder[i];
    }

    spacing[i] = domain / static_cast<RealType>(totalNumberOfSpans);

    origin[i] = -0.5 * spacing[i] * (this->m_SplineOrder[i] - 1);
  }
  origin = this->m_Direction * origin;

  for (unsigned int i = 0; i < ImageDimension; ++i)
  {
    origin[i] += this->m_Origin[i];
  }

  this->m_PhiLattice->SetOrigin(origin);
  this->m_PhiLattice->SetSpacing(spacing);
  this->m_PhiLattice->SetDirection(this->m_Direction);

  this->ProcessObject::SetNthOutput(1, this->m_PhiLattice.GetPointer());
}

template <typename TInputPointSet, typename TOutputImage>
auto
BSplineScatteredDataPointSetToImageFilter<TInputPointSet, TOutputImage>::NumberToIndex(const unsigned int number,
                                                                                       const SizeType size) -> IndexType
{
  IndexType k;
  k[0] = 1;

  for (unsigned int i = 1; i < ImageDimension; ++i)
  {
    k[i] = size[ImageDimension - i - 1] * k[i - 1];
  }

  unsigned int numberModulo = number;

  IndexType index;
  for (unsigned int i = 0; i < ImageDimension; ++i)
  {
    index[ImageDimension - i - 1] = static_cast<unsigned int>(numberModulo / k[ImageDimension - i - 1]);
    numberModulo %= k[ImageDimension - i - 1];
  }
  return index;
}

template <typename TInputPointSet, typename TOutputImage>
void
BSplineScatteredDataPointSetToImageFilter<TInputPointSet, TOutputImage>::PrintSelf(std::ostream & os,
                                                                                   Indent         indent) const
{
  using namespace print_helper;

  Superclass::PrintSelf(os, indent);

  os << indent << "Do multi level: " << this->m_DoMultilevel << std::endl;
  os << indent << "Generate output image: " << this->m_GenerateOutputImage << std::endl;
  os << indent << "Use point weights: " << this->m_UsePointWeights << std::endl;
  os << indent << "Maximum number of levels: " << this->m_MaximumNumberOfLevels << std::endl;
  os << indent << "Current level: " << this->m_CurrentLevel << std::endl;
  os << indent << "Number of control points: " << this->m_NumberOfControlPoints << std::endl;
  os << indent << "Current number of control points: " << this->m_CurrentNumberOfControlPoints << std::endl;
  os << indent << "Close dimension: " << this->m_CloseDimension << std::endl;
  os << indent << "B-spline order: " << this->m_SplineOrder << std::endl;
  os << indent << "Number of levels: " << this->m_NumberOfLevels << std::endl;

  itkPrintSelfObjectMacro(PointWeights);
  itkPrintSelfObjectMacro(PhiLattice);
  itkPrintSelfObjectMacro(PsiLattice);

  os << indent << "Refined lattice coefficients: " << std::endl;
  for (unsigned int i = 0; i < ImageDimension; ++i)
  {
    os << indent << '[' << i << "]: " << this->m_RefinedLatticeCoefficients[i] << std::endl;
  }

  itkPrintSelfObjectMacro(ResidualPointSetValues);

  os << indent << "Kernel: " << std::endl;
  for (unsigned int i = 0; i < ImageDimension; ++i)
  {
    this->m_Kernel[i]->Print(os, indent);
  }

  itkPrintSelfObjectMacro(KernelOrder0);
  itkPrintSelfObjectMacro(KernelOrder1);
  itkPrintSelfObjectMacro(KernelOrder2);
  itkPrintSelfObjectMacro(KernelOrder3);

  os << indent << "Omega lattice per thread: " << m_OmegaLatticePerThread << std::endl;
  os << indent << "Delta lattice per thread: " << m_DeltaLatticePerThread << std::endl;
}
} // end namespace itk

#endif
