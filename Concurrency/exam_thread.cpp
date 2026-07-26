#include <iostream>
#include <vector>
#include <thread>
#include <numeric>
#include <chrono>

void partialSumWorker(const std::vector<long long> &arr, size_t start, size_t end, long long &reuslt)
{
    long long sum = 0;
    for (size_t i = start; i < end; ++i)
    {
        sum += arr[i];
    }
    reuslt = sum;
}

int main()
{
    const size_t N = 100000000;
    std::vector<long long> data(N, 1); // khoi tao tat ca cac gia tri bang 1

    // tinh tuan tu normal
    auto startSeq = std::chrono::high_resolution_clock::now();
    long long seqSum = std::accumulate(data.begin(), data.end(), 0LL);
    auto endSeq = std::chrono::high_resolution_clock::now();

    std::cout << "SUM: " << seqSum << "Time:" << std::chrono::duration_cast<std::chrono::milliseconds>(endSeq - startSeq).count() << "ms\n";

    // tinh song song parallel voi 4 threads

    const unsigned int numThreads = 4;
    std::vector<std::thread> threads;
    std::vector<long long> partialResults(numThreads, 0);

    size_t chunkSize = N / numThreads;

    auto startPar = std::chrono::high_resolution_clock::now();
    for (unsigned int i = 0; i < numThreads; ++i)
    {
        size_t startIdx = i * chunkSize;
        size_t endIdx = (i == numThreads - 1) ? N : (startIdx + chunkSize);

        // truyen tham chieu vao thread
        threads.emplace_back(
            partialSumWorker,
            std::cref(data),
            startIdx,
            endIdx,
            std::ref(partialResults[i]));
        }
    for (auto &th : threads)
    {
        if (th.joinable())
        {
            th.join();
        }
    }

    long long parSum = std::accumulate(partialResults.begin(), partialResults.end(), 0LL);

    auto endPar = std::chrono::high_resolution_clock::now();
    std::cout << "SUM: " << seqSum << "Time:" << std::chrono::duration_cast<std::chrono::milliseconds>(endPar - startPar).count() << "ms\n";
    return 0;
}
