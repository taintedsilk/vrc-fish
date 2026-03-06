#pragma once

#include <string>
#include <vector>

struct MlpLayer {
	int in_dim, out_dim;
	std::vector<double> weights; // out_dim x in_dim (row-major)
	std::vector<double> bias;    // out_dim
};

struct MlpModel {
	std::vector<MlpLayer> layers;
	std::vector<double> norm_mean;
	std::vector<double> norm_std;
	bool loaded = false;
};

// Global model instance
extern MlpModel g_mlpModel;

bool loadMlpWeights(const std::string& path, MlpModel& model);
