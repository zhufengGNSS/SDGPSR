#ifndef SRC_SDGPSR_H_
#define SRC_SDGPSR_H_

#include <queue>
#include <unordered_map>
#include <eigen3/Eigen/Dense>
#include <atomic>
#include "CaCode.h"
#include "FFT.h"
#include "SignalTracker.h"

using Eigen::MatrixXd;
using Eigen::VectorXd;
using Eigen::Vector4d;

const uint64_t GPS_L1_HZ = 1575420000;
const double SAT_FOUND_THRESH = 10.0;
const double SPEED_OF_LIGHT_MPS = 299792458.0;

/*
 * Software-Defined Global Positioning System Receiver (SDGPSR) runs at sample rate fs, and takes in baseband IQ data in 1ms intervals
 * using the basebandSignal function. The data should be roughly centered around clockOffset. A minimum of about 35 seconds worth of data
 * is required to get a navigation solution, as each frame takes 30 seconds, and some data is used in the search and tracker initialization
 * processes. SDGPSR launches its own threads to handle calculation, so the functions here return immediately.
 */

class SDGPSR {
public:
    //fs = data clock rate, clock offset is used to inform system of a hardware clock error
    SDGPSR(double fs, double clockOffsetHz);

    virtual ~SDGPSR();

    //Enque data for processing. Processing is handled by threads of this
    //class, so this returns after data is copied into the queue
    void basebandSignal(const fftwVector &data);

    void basebandSignal(fftwVector &&data);

    //Check to see if all of the enqued data has been processed
    bool synced(void);

    //Get user position in WGS84 ECEF frame (m)
    Vector3d positionECEF(void);

    //Get user position in Lat/Lon/Alt (deg,deg,m)
    Vector3d positionLLA(void);

    //Get user GPS time of week (s)
    double timeOfWeek(void);

	//Get the state of all tracked satellites. Pair is PRN/State
    std::vector<std::pair<unsigned, State>> trackingStatus(void);

	//Flag to indicate the nav solution has started
    bool navSolution(void);

private:
    //Compute Nav Solution
    void solve(void);

    //Search for satellites, create Signal Trackers, then continuously pull data from input queue and
    //send out to Signal Trackers for processing, then compute Nav Solution
    void threadFunction();

    //Compute corrCount iterations of a circular correlation between the frequency-domain searchData and time-domain basebandcode,
    //returning the summed magnitides for each code offset
    std::vector<double> nonCoherentCorrelator(std::vector<fftwVector> &searchData, fftwVector &basebandCode, unsigned corrCount);

    //Generate a prn sequence with the given baseband frequency
    void basebandGenerator(unsigned prn, fftwVector &basebandCode, double freqOffset);

    //Conduct a 2-d search for the given prn using the frequency-domain searchData for all chip offsets and frequencies
    //between the start and stop at the given step size. The search will be conducted non-coherently using corrCount integrations
    SearchResult search(std::vector<fftwVector> &searchData,
                        unsigned prn,
                        unsigned corrCount,
                        double freqStart,
                        double freqStop,
                        double freqStep);

    //WGS84 ECEF position (m) in x-z, w is GPS time of week (s). w follows z in the vector
    Vector4d userEstimateEcefTime_;
    //Mutex should be locked any time a public function reads from userEstimateEcefTime_, and any time it is written to
    std::mutex userEstMutex_;

    bool navSolutionStarted_;

    double fs_;

    //Queue of 1-ms chunks of input data
    std::queue<fftwVector> input_;
    //Mutex should be locked any time a public function reads from input_, and any time it is modified
    std::mutex inputMutex_;

    FFT fft_;

    std::atomic_bool run_;

    std::atomic_bool synced_;

    std::list<std::unique_ptr<SignalTracker>> channels_;
    //Mutex should be locked any time a public function reads from channels_, and any time it is modified
    std::mutex channelMutex_;

    double clockOffset_;

    std::thread signalProcessor_;

#ifdef DEBUG_FILES
    std::ofstream userEstimates_;
    std::unordered_map<unsigned,std::ofstream> residualsOutput_;
#endif
};

#endif /* SRC_SDGPSR_H_ */
