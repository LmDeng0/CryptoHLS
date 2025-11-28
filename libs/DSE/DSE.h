// #ifndef CRYPTOHLS_DSE_H
// #define CRYPTOHLS_DSE_H

// #include "Core.h"
// #include <random>

// class CostModel {
// public:
//     CostModel(const FunctionCost& base);

//     // [修正] 确保函数声明末尾有 const，以匹配实现
//     FunctionCost predictCost(const Solution& solution) const;

// private:
//     FunctionCost base_cost;
// };

// Solution generateRandomSolution(const DesignSpace& designSpace, std::mt19937& rng);

// #endif // CRYPTOHLS_DSE_H

#ifndef CRYPTOHLS_DSE_H
#define CRYPTOHLS_DSE_H

#include "Core.h"  // 包含DesignCommand定义
#include <random>

class CostModel {
public:
    CostModel(const FunctionCost& base);
    
    // 确保函数声明末尾有const
    FunctionCost predictCost(const Solution& solution) const;

private:
    FunctionCost base_cost;
};

// 只声明函数，不实现
Solution generateRandomSolution(const DesignSpace& designSpace, std::mt19937& rng);

#endif // CRYPTOHLS_DSE_H