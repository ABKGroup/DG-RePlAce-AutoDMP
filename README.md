# DG-RePlAce Parameter Autotuner 

* Built upon the GPU-accelerated global placer [DG-RePlAce](https://arxiv.org/abs/2404.13049) and the [OpenROAD](https://github.com/The-OpenROAD-Project) project.
* Automatic parameter tuning based on multi-objective hyperparameter Bayesian optimization (MOBO) from [AutoDMP](https://github.com/NVlabs/AutoDMP).

# Dependency 

- GPU architecture compatibility 6.0 or later
    - Code has been tested on GPUs with compute compatibility 8.0 on DGX A100 machine. 

# How to Build 
```
cd OpenROAD-tool
mkdir build
cd build
build -j 40
```


# How to Run Multi-Objective Bayesian Optimization

To run the test of multi-objective Bayesian optimization on Swerv_Wrapper NanGate45, call:
```
source run_me.sh
```


