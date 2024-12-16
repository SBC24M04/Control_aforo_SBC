#pragma once

#include "dl_base_shape.hpp"
#include "dl_base_sub.hpp"
#include "dl_base_sub4d.hpp"
#include "dl_module_base.hpp"

namespace dl {
namespace module {
/**
 * NOTE: addition is element-wise, i.e., output[i,j,k] = input0[i,j,k] + input1[i,j,k]
 *
 * @tparam feature_t supports int16_t and int8_t,
 *         - int16_t: stands for operation in int16_t quantize
 *         - int8_t: stands for operation in int8_t quantize
 */
class Sub4D : public Module {
public:
    /**
     * @brief Construct a new Sub4D object.
     *
     * @param name            name of module
     * @param inplace         inplace type.
     */
    Sub4D(const char *name = NULL,
          module_inplace_t inplace = MODULE_NON_INPLACE,
          quant_type_t quant_type = QUANT_TYPE_NONE) :
        Module(name, inplace, quant_type) // 调用基类构造函数
    {
    }

    /**
     * @brief Destroy the Sub4D object.
     */
    ~Sub4D() {}

    std::vector<std::vector<int>> get_output_shape(std::vector<std::vector<int>> &input_shapes)
    {
        assert(input_shapes.size() == 2);

        // 调用基类的多方向广播函数来支持4D
        std::vector<int> output_shape = base::get_multidirectional_broadcasting_shape(input_shapes[0], input_shapes[1]);

        return std::vector<std::vector<int>>(1, output_shape);
    }

    void forward(std::vector<dl::TensorBase *> &tensors, runtime_mode_t mode)
    {
        if (quant_type == QUANT_TYPE_SYMM_8BIT) {
            forward_template<int8_t>(tensors, mode);
        } else if (quant_type == QUANT_TYPE_SYMM_16BIT) {
            forward_template<int16_t>(tensors, mode);
        }
    }

    void forward_args(void *args)
    {
        if (quant_type == QUANT_TYPE_SYMM_8BIT) {
            base::sub4d<int8_t>(args); // 4D 版本的 add 运算
        } else if (quant_type == QUANT_TYPE_SYMM_16BIT) {
            base::sub4d<int16_t>(args); // 4D 版本的 add 运算
        }
    }

    template <typename T>
    void forward_template(std::vector<TensorBase *> &tensors, runtime_mode_t mode)
    {
        TensorBase *input0 = tensors[m_inputs_index[0]];
        TensorBase *input1 = tensors[m_inputs_index[1]];
        TensorBase *output = tensors[m_outputs_index[0]];

        // 获取用于4D加法运算的参数
        std::vector<base::arithArgsType<T>> m_args =
            base::get_arith_operation_args<T>(output, input0, input1, Linear, nullptr, mode);
        int task_size = m_args.size();
        if (task_size == 1) {
            forward_args((void *)&m_args[0]); // 单任务
        } else if (task_size == 2) {
            module_forward_dual_core(this, (void *)&m_args[0], (void *)&m_args[1]); // 多任务
        } else {
            ESP_LOGE("Sub4D", "Only support task size is 1 or 2, currently task size is %d", task_size);
        }
    }

    /**
     * @brief deserialize Sub4D module instance by node serialization information
     */
    static Module *deserialize(fbs::FbsModel *fbs_model, std::string node_name)
    {
        Module *op = nullptr;
        quant_type_t quant_type;
        fbs_model->get_operation_attribute(node_name, "quant_type", quant_type);

        //
        if (quant_type == QUANT_TYPE_SYMM_8BIT || quant_type == QUANT_TYPE_SYMM_16BIT) {
            op = new Sub4D(NULL, MODULE_NON_INPLACE, quant_type);
        }
        return op;
    }

    void print() { ESP_LOGI("Sub4D", "quant_type: %s.", quant_type_to_string(quant_type)); }
    // void print() { ESP_LOGI("Sub4D", "quant_type: %s, input0_sahep: %s.", quant_type_to_string(quant_type),
    // shape_to_string().c_str()); }
};

/**
 * @brief: Performs element-wise binary subtraction (with Numpy-style broadcasting support).
 *         Please refer to https://onnx.ai/onnx/operators/onnx__Sub.html for more details
 *
 * @tparam feature_t supports int16_t and int8_t,
 *         - int16_t: stands for operation in int16_t quantize
 *         - int8_t: stands for operation in int8_t quantize
 */
class Sub : public Module {
public:
    /**
     * @brief Construct a new Sub object.
     *
     * @param name            name of module
     * @param inplace         inplace type.
     */
    Sub(const char *name = NULL,
        module_inplace_t inplace = MODULE_NON_INPLACE,
        quant_type_t quant_type = QUANT_TYPE_NONE) :
        Module(name, inplace, quant_type)
    {
    }

    /**
     * @brief Destroy the Sub object.
     */
    ~Sub() {}

    std::vector<std::vector<int>> get_output_shape(std::vector<std::vector<int>> &input_shapes)
    {
        assert(input_shapes.size() == 2);

        std::vector<int> output_shape = base::get_multidirectional_broadcasting_shape(input_shapes[0], input_shapes[1]);

        return std::vector<std::vector<int>>(1, output_shape);
    }

    void forward(std::vector<dl::TensorBase *> &tensors, runtime_mode_t mode)
    {
        if (quant_type == QUANT_TYPE_SYMM_8BIT) {
            forward_template<int8_t>(tensors, mode);
        } else if (quant_type == QUANT_TYPE_SYMM_16BIT) {
            forward_template<int16_t>(tensors, mode);
        }
    }

    void forward_args(void *args)
    {
        if (quant_type == QUANT_TYPE_SYMM_8BIT) {
            base::elemwise_sub((base::elemwiseArgsType<int8_t> *)args);
        } else if (quant_type == QUANT_TYPE_SYMM_16BIT) {
            base::elemwise_sub((base::elemwiseArgsType<int16_t> *)args);
        }
    }

    template <typename T>
    void forward_template(std::vector<TensorBase *> &tensors, runtime_mode_t mode)
    {
        TensorBase *input0 = tensors[m_inputs_index[0]];
        TensorBase *input1 = tensors[m_inputs_index[1]];
        TensorBase *output = tensors[m_outputs_index[0]];

        std::vector<base::elemwiseArgsType<T>> m_args =
            base::get_elemwise_operation_args<T>(output, input0, input1, mode); // get element-wise operation args
        int task_size = m_args.size();
        if (task_size == 1) {
            forward_args((void *)&m_args[0]); // single task
        } else if (task_size == 2) {
            module_forward_dual_core(this, (void *)&m_args[0], (void *)&m_args[1]); // two task
        } else {
            ESP_LOGE("Sub", "Only support task size is 1 or 2, currently task size is %d", task_size);
        }
    }

    /**
     * @brief deserialize Sub module instance by node serialization information
     */
    static Module *deserialize(fbs::FbsModel *fbs_model, std::string node_name)
    {
        Module *op = nullptr;
        quant_type_t quant_type;
        fbs_model->get_operation_attribute(node_name, "quant_type", quant_type);

        //
        if (quant_type == QUANT_TYPE_SYMM_8BIT || quant_type == QUANT_TYPE_SYMM_16BIT) {
            op = new Sub(NULL, MODULE_NON_INPLACE, quant_type);
        }
        return op;
    }

    void print() { ESP_LOGI("Sub", "quant_type: %s.", quant_type_to_string(quant_type)); }
    // void print() { ESP_LOGI("Sub", "quant_type: %s, input0_sahep: %s.", quant_type_to_string(quant_type),
    // shape_to_string().c_str()); }
};

} // namespace module
} // namespace dl
