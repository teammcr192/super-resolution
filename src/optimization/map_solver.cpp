#include "optimization/map_solver.h"

#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "optimization/regularizer.h"

#include "glog/logging.h"

namespace super_resolution {

void MapSolverOptions::AdjustThresholdsAdaptively(
    const int num_parameters, const double regularization_parameter_sum) {

  const double threshold_scale = num_parameters * regularization_parameter_sum;
  if (threshold_scale < 1.0) {
    return;  // Only scale up if needed, not down.
  }
  gradient_norm_threshold *= threshold_scale;
  cost_decrease_threshold *= threshold_scale;
  parameter_variation_threshold *= threshold_scale;
}

void MapSolverOptions::PrintSolverOptions() const {
  std::string solver_name = "conjugate gradient";
  if (least_squares_solver == LBFGS_SOLVER) {
    solver_name = "LBFGS";
  }
  std::cout << "  Least squares solver:                "
            << solver_name;
  if (use_numerical_differentiation) {
    std::cout << " (numerical differentiation [step = "
              << numerical_differentiation_step << "])" << std::endl;
  } else {
    std::cout << " (analytical differentiation)" << std::endl;
  }
  if (split_channels) {
    std::cout << "  Channel splitting enabled." << std::endl;
  }
  std::cout << "  Threshold 1 (gradient norm):         "
            << gradient_norm_threshold << std::endl;
  std::cout << "  Threshold 2 (cost decrease):         "
            << cost_decrease_threshold << std::endl;
  std::cout << "  Threshold 3 (parameter variation):   "
            << parameter_variation_threshold << std::endl;
}

MapSolver::MapSolver(
    const ImageModel& image_model,
    const std::vector<ImageData>& low_res_images,
    const bool print_solver_output)
    : Solver(image_model, print_solver_output) {

  const int num_observations = low_res_images.size();
  CHECK_GT(num_observations, 0)
      << "Cannot super-resolve with 0 low-res images.";

  // Set number of channels, and verify that this is consistent among all of
  // the given low-res images.
  num_channels_ = low_res_images[0].GetNumChannels();
  for (int i = 1; i < low_res_images.size(); ++i) {
    CHECK_EQ(low_res_images[i].GetNumChannels(), num_channels_)
        << "Image channel counts do not match up.";
  }

  // Set the size of the HR images. There must be at least one image at
  // low_res_images[0], otherwise the above check will have failed.
  const int upsampling_scale = image_model_.GetDownsamplingScale();
  const cv::Size lr_image_size = low_res_images[0].GetImageSize();
  image_size_ = cv::Size(
      lr_image_size.width * upsampling_scale,
      lr_image_size.height * upsampling_scale);

  // Rescale the LR observations to the HR image size so they're useful for in
  // the objective function.
  observations_.reserve(num_observations);
  for (const ImageData& low_res_image : low_res_images) {
    ImageData observation = low_res_image;  // copy
    observation.ResizeImage(image_size_, INTERPOLATE_NEAREST);
    observations_.push_back(observation);
  }
}

void MapSolver::AddRegularizer(
    std::shared_ptr<Regularizer> regularizer,
    const double regularization_parameter) {

  regularizers_.push_back(
      std::make_pair(regularizer, regularization_parameter));
}

int MapSolver::GetNumDataPoints() const {
  const long num_data_points = GetNumPixels() * GetNumChannels();
  CHECK_LE(num_data_points, std::numeric_limits<int>::max())
      << "Number of data points exceeds maximum size.";
  return GetNumPixels() * GetNumChannels();
}

double MapSolver::GetRegularizationParameterSum() const {
  double regularization_parameter_sum = 0.0;
  for (const auto& regularizer_and_parameter : regularizers_) {
    regularization_parameter_sum += regularizer_and_parameter.second;
  }
  return regularization_parameter_sum;
}

}  // namespace super_resolution
