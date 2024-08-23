/*******************************************************************************
 * Copyright (c) 2022 - 2024 NVIDIA Corporation & Affiliates.                  *
 * All rights reserved.                                                        *
 *                                                                             *
 * This source code and the accompanying materials are made available under    *
 * the terms of the Apache License 2.0 which accompanies this distribution.    *
 ******************************************************************************/
#include "common/Logger.h"
#include "common/RestClient.h"
#include "common/ServerHelper.h"
#include "cudaq/Support/Version.h"
#include "cudaq/utils/cudaq_utils.h"
#include <bitset>
#include <fstream>
#include <map>
#include <thread>

namespace cudaq {

/// @brief The FermioniqServerHelper class extends the ServerHelper class to handle
/// interactions with the Fermioniq server for submitting and retrieving quantum
/// computation jobs.
class FermioniqServerHelper : public ServerHelper {

  static constexpr int POLLING_INTERVAL_IN_SECONDS = 1;

  static constexpr const char *DEFAULT_URL = "https://fermioniq-api-fapp-prod.azurewebsites.net";
  static constexpr const char *DEFAULT_API_KEY = "gCUVmJOKVCdPKRYpgk7nNWM_kTAsZfPeYTbte2sNuKtXAzFuYdj9ag==";

  static constexpr const char *CFG_URL_KEY = "base_url";
  static constexpr const char *CFG_ACCESS_TOKEN_ID_KEY = "access_token_id";
  static constexpr const char *CFG_ACCESS_TOKEN_SECRET_KEY = "access_token_secret";
  static constexpr const char *CFG_API_KEY_KEY = "access_token_secret";
  static constexpr const char *CFG_USER_AGENT_KEY = "access_token_secret";
  static constexpr const char *CFG_TOKEN_KEY = "token";
  
  static constexpr const char *CFG_REMOTE_CONFIG_KEY = "remote_config";
  static constexpr const char *CFG_NOISE_MODEL_KEY = "noise_model";


public:
  /// @brief Returns the name of the server helper.
  const std::string name() const override { return "Fermioniq"; }

  /// @brief Returns the headers for the server requests.
  RestHeaders getHeaders() override;

  /// @brief Initializes the server helper with the provided backend
  /// configuration.
  void initialize(BackendConfig config) override;

  /// @brief Creates a quantum computation job using the provided kernel
  /// executions and returns the corresponding payload.
  ServerJobPayload
  createJob(std::vector<KernelExecution> &circuitCodes) override;

  /// @brief Extracts the job ID from the server's response to a job submission.
  std::string extractJobId(ServerMessage &postResponse) override;

  /// @brief Constructs the URL for retrieving a job based on the server's
  /// response to a job submission.
  std::string constructGetJobPath(ServerMessage &postResponse) override;

  /// @brief Constructs the URL for retrieving a job based on a job ID.
  std::string constructGetJobPath(std::string &jobId) override;

  /// @brief Checks if a job is done based on the server's response to a job
  /// retrieval request.
  bool jobIsDone(ServerMessage &getJobResponse) override;

  /// @brief Processes the server's response to a job retrieval request and
  /// maps the results back to sample results.
  cudaq::sample_result processResults(ServerMessage &postJobResponse,
                                      std::string &jobId) override;

#if 0
  /// @brief Update `passPipeline` with architecture-specific pass options
  void updatePassPipeline(const std::filesystem::path &platformPath,
                          std::string &passPipeline) override;
#endif

  /// @brief Return next results polling interval
  std::chrono::microseconds nextResultPollingInterval(ServerMessage &postResponse) override;

private:
  /// @brief RestClient used for HTTP requests.
  RestClient client;

  /// @brief Helper method to retrieve the value of an environment variable.
  std::string getEnvVar(const std::string &key, const std::string &defaultVal,
                        const bool isRequired) const;

  /// @brief Helper function to get value from config or return a default value.
  std::string getValueOrDefault(const BackendConfig &config,
                                const std::string &key,
                                const std::string &defaultValue) const;

  /// @brief Helper method to check if a key exists in the configuration.
  bool keyExists(const std::string &key) const;
};

// Initialize the Fermioniq server helper with a given backend configuration
void FermioniqServerHelper::initialize(BackendConfig config) {
  cudaq::info("Initializing Fermioniq Backend.");

  backendConfig[CFG_URL_KEY] = getValueOrDefault(config, "base_url", DEFAULT_URL);
  backendConfig[CFG_API_KEY_KEY] = getValueOrDefault(config, "api_key", DEFAULT_API_KEY);
  
  backendConfig[CFG_ACCESS_TOKEN_ID_KEY] = getEnvVar("FERMIONIQ_ACCESS_TOKEN_ID", "", true);
  backendConfig[CFG_ACCESS_TOKEN_SECRET_KEY] = getEnvVar("FERMIONIQ_ACCESS_TOKEN_SECRET", "", true);

  backendConfig[CFG_USER_AGENT_KEY] = "cudaq/" + std::string(cudaq::getVersion());

  if (config.find("remote_config") != config.end())
    backendConfig[CFG_REMOTE_CONFIG_KEY] = config["remote_config"];
  if (config.find("noise_model") != config.end())
    backendConfig[CFG_NOISE_MODEL_KEY] = config["noise_model"];
}

// Implementation of the getValueOrDefault function
std::string
FermioniqServerHelper::getValueOrDefault(const BackendConfig &config,
                                    const std::string &key,
                                    const std::string &defaultValue) const {
  return config.find(key) != config.end() ? config.at(key) : defaultValue;
}

// Retrieve an environment variable
std::string FermioniqServerHelper::getEnvVar(const std::string &key,
                                        const std::string &defaultVal,
                                        const bool isRequired) const {
  // Get the environment variable
  const char *env_var = std::getenv(key.c_str());
  // If the variable is not set, either return the default or throw an exception
  if (env_var == nullptr) {
    if (isRequired)
      throw std::runtime_error("The " + key + " environment variable is not set but is required.");
    else
      return defaultVal;
  }
  // Return the variable as a string
  return std::string(env_var);
}

// Helper function to get a value from a dictionary or return a default
template <typename K, typename V>
V getOrDefault(const std::unordered_map<K, V> &map, const K &key,
               const V &defaultValue) {
  auto it = map.find(key);
  return (it != map.end()) ? it->second : defaultValue;
}

// Check if a key exists in the backend configuration
bool FermioniqServerHelper::keyExists(const std::string &key) const {
  return backendConfig.find(key) != backendConfig.end();
}

// Create a job for the Fermioniq quantum computer
ServerJobPayload
FermioniqServerHelper::createJob(std::vector<KernelExecution> &circuitCodes) {
  cudaq::debug("createJob");
  std::vector<ServerMessage> jobs;
  for (auto &circuitCode : circuitCodes) {
    // Construct the job message (for Fermioniq backend)
    ServerMessage job;
    job["circuit"] = circuitCode.code;
    if (keyExists(CFG_REMOTE_CONFIG_KEY)) {
      job["remote_config"] = backendConfig.at(CFG_REMOTE_CONFIG_KEY);
    }

    jobs.push_back(job);

    //cudaq::debug("post job: {}", job);
  }

  // Return a tuple containing the job path, headers, and the job message
  auto job_path = backendConfig.at(CFG_URL_KEY) + "/api/jobs";
  auto ret = std::make_tuple(job_path, getHeaders(), jobs);
  return ret;
}

bool FermioniqServerHelper::jobIsDone(ServerMessage &getJobResponse) {
  cudaq::debug("jobIsDone from {}", getJobResponse);
  // TO-DO: Check if job is done
  return false;
}

// From a server message, extract the job ID
std::string FermioniqServerHelper::extractJobId(ServerMessage &postResponse) {
  cudaq::debug("extractJobId from {}", postResponse);

  return "";
}

// Construct the path to get a job
std::string FermioniqServerHelper::constructGetJobPath(ServerMessage &postResponse) {
  cudaq::debug("constructGetJobPath from {}", postResponse);
  // todo: Extract job-id from postResponse
  
  auto ret = backendConfig.at(CFG_URL_KEY) + "/api/jobs/";
  return ret;
}

// Overloaded version of constructGetJobPath for jobId input
std::string FermioniqServerHelper::constructGetJobPath(std::string &jobId) {
  cudaq::debug("constructGetJobPath (jobId) from {}", jobId);
  
  auto ret = backendConfig.at(CFG_URL_KEY) + "/api/jobs/";
  return ret;
}

// Process the results from a job
cudaq::sample_result
FermioniqServerHelper::processResults(ServerMessage &postJobResponse,
                                 std::string &jobID) {
  cudaq::debug("processResults for job: {} - {}", jobID, postJobResponse.dump());
  auto ret = cudaq::sample_result();
  return ret;
}

// Get the headers for the API requests
RestHeaders FermioniqServerHelper::getHeaders() {
  cudaq::debug("getHeaders");
  // Construct the headers
  RestHeaders headers;
  if (keyExists(CFG_API_KEY_KEY) && backendConfig.at(CFG_API_KEY_KEY) != "") {
    headers["x-functions-key"] = backendConfig.at(CFG_API_KEY_KEY);
  }
  
  if (keyExists(CFG_TOKEN_KEY)) {
    headers["Authorization"] = "Bearer " + backendConfig.at(CFG_TOKEN_KEY);
  }
  headers["Content-Type"] = "application/json";
  headers["User-Agent"] = backendConfig.at(CFG_USER_AGENT_KEY);

  // Return the headers
  return headers;
}

#if 0
// TO-DO Fermioniq: Note sure if we need this
void FermioniqServerHelper::updatePassPipeline(
  const std::filesystem::path &platformPath, std::string &passPipeline) {
  // Note: the leading and trailing single quotes are needed in case there are
  // spaces in the filename.
  std::string pathToFile;
  auto iter = backendConfig.find("mapping_file");
  if (iter != backendConfig.end()) {
    // Use provided path to file
    pathToFile = std::string("'") + iter->second + std::string("'");
  } else {
    // Construct path to file
    pathToFile =
        std::string("'") +
        std::string(platformPath / std::string("mapping/iqm") /
                    (backendConfig["qpu-architecture"] + std::string(".txt'")));
  }
  passPipeline =
      std::regex_replace(passPipeline, std::regex("%QPU_ARCH%"), pathToFile);
}
#endif

std::chrono::microseconds
FermioniqServerHelper::nextResultPollingInterval(ServerMessage &postResponse) {
  return std::chrono::seconds(POLLING_INTERVAL_IN_SECONDS); // jobs never take less than few seconds
};

} // namespace cudaq

// Register the Fermioniq server helper in the CUDA-Q server helper factory
CUDAQ_REGISTER_TYPE(cudaq::ServerHelper, cudaq::FermioniqServerHelper, fermioniq)