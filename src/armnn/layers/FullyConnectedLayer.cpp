//
// Copyright © 2017 Arm Ltd and Contributors. All rights reserved.
// SPDX-License-Identifier: MIT
//
#include "FullyConnectedLayer.hpp"

#include "LayerCloneBase.hpp"

#include <armnn/TypesUtils.hpp>
#include <backendsCommon/TensorHandle.hpp>
#include <backendsCommon/WorkloadData.hpp>
#include <backendsCommon/WorkloadFactory.hpp>

namespace armnn
{

FullyConnectedLayer::FullyConnectedLayer(const FullyConnectedDescriptor& param, const char* name)
    : LayerWithParameters(param.GetNumViews(), 1, LayerType::FullyConnected, param, name)
{
}

std::unique_ptr<IWorkload> FullyConnectedLayer::CreateWorkload(const IWorkloadFactory& factory) const
{
    // on this level constant data should not be released..
    FullyConnectedQueueDescriptor descriptor;
    if (m_Param.m_ConstantWeights)
    {
        ARMNN_ASSERT_MSG(m_Weight != nullptr, "FullyConnectedLayer: Weights data should not be null.");
        descriptor.m_Weight = m_Weight.get();

        if (m_Param.m_BiasEnabled)
        {
            ARMNN_ASSERT_MSG(m_Bias != nullptr, "FullyConnectedLayer: Bias data should not be null.");
            descriptor.m_Bias = m_Bias.get();
        }
    }
    SetAdditionalInfo(descriptor);

    return factory.CreateFullyConnected(descriptor, PrepInfoAndDesc(descriptor));
}

FullyConnectedLayer* FullyConnectedLayer::Clone(Graph& graph) const
{
    auto layer = CloneBase<FullyConnectedLayer>(graph, m_Param, GetName());
    if (m_Param.m_ConstantWeights)
    {
        layer->m_Weight = m_Weight ? m_Weight : nullptr;

        if (layer->m_Param.m_BiasEnabled)
        {
            layer->m_Bias = m_Bias ? m_Bias : nullptr;
        }
    }
    return std::move(layer);
}

std::vector<TensorShape> FullyConnectedLayer::InferOutputShapes(const std::vector<TensorShape>& inputShapes) const
{
    ARMNN_ASSERT(inputShapes.size() == 2);
    const TensorShape& inputShape = inputShapes[0];
    const TensorShape weightShape = inputShapes[1];

    // Output for FC is [1, w[1]].
    unsigned int batches = inputShape[0];
    unsigned int dimIdx = m_Param.m_TransposeWeightMatrix ? 0 : 1;

    return std::vector<TensorShape>({ TensorShape({batches, weightShape[dimIdx]})});
}

void FullyConnectedLayer::ValidateTensorShapesFromInputs()
{
    const TensorShape& outputShape = GetOutputSlot(0).GetTensorInfo().GetShape();

    VerifyShapeInferenceType(outputShape, m_ShapeInferenceMethod);

    std::vector<TensorShape> inferredShapes;
    if (m_Param.m_ConstantWeights)
    {
        // check if m_Weight data is not nullptr
        ARMNN_ASSERT_MSG(m_Weight != nullptr, "FullyConnectedLayer: Weights data should not be null.");

        inferredShapes = InferOutputShapes({GetInputSlot(0).GetConnection()->GetTensorInfo().GetShape(),
                                            m_Weight->GetTensorInfo().GetShape()});
    }
    else
    {
        inferredShapes = InferOutputShapes({GetInputSlot(0).GetConnection()->GetTensorInfo().GetShape(),
                                            GetInputSlot(1).GetConnection()->GetTensorInfo().GetShape()});
    }

    ARMNN_ASSERT(inferredShapes.size() == 1);
    ARMNN_ASSERT(inferredShapes[0].GetDimensionality() == Dimensionality::Specified);

    ValidateAndCopyShape(outputShape, inferredShapes[0], m_ShapeInferenceMethod, "FullyConnectedLayer");
}

Layer::ConstantTensors FullyConnectedLayer::GetConstantTensorsByRef()
{
    return {m_Weight, m_Bias};
}

void FullyConnectedLayer::Accept(ILayerVisitor& visitor) const
{
    Optional<ConstTensor> optionalWeightsTensor = EmptyOptional();
    Optional<ConstTensor> optionalBiasTensor = EmptyOptional();

    ManagedConstTensorHandle managedWeight(m_Weight);
    ManagedConstTensorHandle managedBias(m_Bias);
    if (GetParameters().m_ConstantWeights)
    {
        ConstTensor weightsTensor(managedWeight.GetTensorInfo(), managedWeight.Map());
        optionalWeightsTensor = Optional<ConstTensor>(weightsTensor);

        if (GetParameters().m_BiasEnabled)
        {
            ConstTensor biasTensor(managedBias.GetTensorInfo(), managedBias.Map());
            optionalBiasTensor = Optional<ConstTensor>(biasTensor);
        }
    }

    visitor.VisitFullyConnectedLayer(this,
                                     GetParameters(),
                                     optionalWeightsTensor.value(),
                                     optionalBiasTensor,
                                     GetName());
}

void FullyConnectedLayer::ExecuteStrategy(IStrategy& strategy) const
{
    std::vector <armnn::ConstTensor> constTensors;
    ManagedConstTensorHandle managedWeight(m_Weight);
    ManagedConstTensorHandle managedBias(m_Bias);

    if(GetParameters().m_ConstantWeights)
    {
        constTensors.emplace_back(ConstTensor(managedWeight.GetTensorInfo(), managedWeight.Map()));
        if (GetParameters().m_BiasEnabled)
        {
            constTensors.emplace_back(ConstTensor(managedBias.GetTensorInfo(), managedBias.Map()));
        }
    }
    strategy.ExecuteStrategy(this, GetParameters(), constTensors, GetName());
}

} // namespace armnn
