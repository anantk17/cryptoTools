#include "LdpcDecoder.h"
#include <cassert>
#include "Mtx.h"
#include "LdpcEncoder.h"
#include "LdpcSampler.h"
#include "LDPC_decoder.h"
#include "rank.h"

namespace osuCrypto {

    void LdpcDecoder::init(SparseMtx& H)
    {
        mH = H;
        auto n = mH.cols();
        auto m = mH.rows();

        assert(n > m);
        mK = n - m;

        mR.resize(m, n);
        mM.resize(m, n);

        mW.resize(n);
    }


    std::vector<u8> LdpcDecoder::bpDecode(span<u8> codeword, u64 maxIter)
    {
        auto n = mH.cols();
        auto m = mH.rows();

        assert(codeword.size() == n);

        std::array<double, 2> wVal{ { mP / (1 - mP), (1 - mP) / mP} };
        auto nan = std::nan("");
        std::fill(mR.begin(), mR.end(), nan);
        std::fill(mM.begin(), mM.end(), nan);

        // #1
        for (u64 i = 0; i < n; ++i)
        {
            assert(codeword[i] < 2);
            mW[i] = wVal[codeword[i]];

            for (auto j : mH.mCols[i])
            {
                mR(j, i) = mW[i];
            }
        }

        std::vector<u8> c(n);
        std::vector<double> rr; rr.reserve(100);
        for (u64 ii = 0; ii < maxIter; ii++)
        {
            // #2
            for (u64 j = 0; j < m; ++j)
            {
                rr.resize(mH.mRows[j].size());
                for (u64 i : mH.mRows[j])
                {
                    // \Pi_{k in Nj \ {i} }  (r_k^j + 1)/(r_k^j - 1)
                    double v = 1;

                    for (u64 k : mH.mRows[j])
                    {
                        if (k != i)
                        {
                            assert(mR(j, k) != nan);

                            auto r = mR(j,k);
                            v *= (r + 1) / (r - 1);
                        }
                    }

                    // m_j^i 
                    mM(j, i) = (v + 1) / (v - 1);
                }
            }

            // i indexes a column, [1,...,n]
            for (u64 i = 0; i < n; ++i)
            {
                // j indexes a row, [1,...,m]
                for (u64 j : mH.mCols[i])
                {
                    // r_i^j = w_i * Pi_{k in Ni \ {j} } m_k^i
                    mR(j, i) = mW[i];

                    // j indexes a row, [1,...,m]
                    for (u64 k : mH.mCols[i])
                    {
                        if (k != j)
                        {
                            //auto mr = mM.rows();
                            //assert(j < mR.rows());
                            //assert(i < mR.cols());
                            //assert(i < mM.rows());
                            //assert(k < mM.cols());
                            assert(mM(k, i) != nan);

                            mR(j, i) *= mM(k, i);
                        }
                    }
                }
            }

            // i indexes a column, [1,...,n]
            for (u64 i = 0; i < n; ++i)
            {
                //L(ci | wi, m^i)
                double L = mW[i];

                // k indexes a row, [1,...,m]
                for (u64 k : mH.mCols[i])
                {
                    assert(mM(k, i) != nan);
                    L *= mM(k, i);
                }

                c[i] = (L >= 1) ? 0 : 1;
            }

            if (check(c))
            {
                c.resize(n - m);
                return c;
            }
        }

        return {};
    }

    double sgn(double x)
    {
        if (x >= 0)
            return 1;
        return -1;
    }

    double phi(double x)
    {
        return -std::log(std::tanh(x * 0.5));
    }

    std::vector<u8> LdpcDecoder::logbpDecode(span<u8> codeword, u64 maxIter)
    {

        auto n = mH.cols();
        auto m = mH.rows();

        assert(codeword.size() == n);

        std::array<double, 2> wVal{
            {std::log(mP / (1 - mP)), 
            std::log((1 - mP) / mP)} };

        auto nan = std::nan("");
        std::fill(mR.begin(), mR.end(), nan);
        std::fill(mM.begin(), mM.end(), nan);

        // #1
        for (u64 i = 0; i < n; ++i)
        {
            assert(codeword[i] < 2);
            mW[i] = wVal[codeword[i]];

            for (auto j : mH.mCols[i])
            {
                mR(j, i) = mW[i];
            }
        }

        std::vector<u8> c(n);
        std::vector<double> rr; rr.reserve(100);
        for (u64 ii = 0; ii < maxIter; ii++)
        {
            // #2
            for (u64 j = 0; j < m; ++j)
            {
                rr.resize(mH.mRows[j].size());
                for (u64 i : mH.mRows[j])
                {
                    // \Pi_{k in Nj \ {i} }  (r_k^j + 1)/(r_k^j - 1)
                    double v = 0;
                    double s = 1;

                    for (u64 k : mH.mRows[j])
                    {
                        if (k != i)
                        {
                            assert(mR(j, k) != nan);

                            v += phi(abs(mR(j, k)));

                            s *= sgn(mR(j, k));
                        }
                    }

                    // m_j^i 
                    mM(j, i) = s * phi(v);
                }
            }

            // i indexes a column, [1,...,n]
            for (u64 i = 0; i < n; ++i)
            {
                // j indexes a row, [1,...,m]
                for (u64 j : mH.mCols[i])
                {
                    // r_i^j = w_i * Pi_{k in Ni \ {j} } m_k^i
                    mR(j, i) = mW[i];

                    // j indexes a row, [1,...,m]
                    for (u64 k : mH.mCols[i])
                    {
                        if (k != j)
                        {
                            //auto mr = mM.rows();
                            //assert(j < mR.rows());
                            //assert(i < mR.cols());
                            //assert(i < mM.rows());
                            //assert(k < mM.cols());
                            assert(mM(k, i) != nan);

                            mR(j, i) += mM(k, i);
                        }
                    }
                }
            }

            // i indexes a column, [1,...,n]
            for (u64 i = 0; i < n; ++i)
            {
                //log L(ci | wi, m^i)
                double L = mW[i];

                // k indexes a row, [1,...,m]
                for (u64 k : mH.mCols[i])
                {
                    assert(mM(k, i) != nan);
                    L += mM(k, i);
                }

                c[i] = (L >= 0) ? 0 : 1;
            }

            if (check(c))
            {
                c.resize(n - m);
                return c;
            }
        }

        return {};
    }

    std::vector<u8> LdpcDecoder::minSumDecode(span<u8> codeword, u64 maxIter)
    {

        auto n = mH.cols();
        auto m = mH.rows();

        assert(codeword.size() == n);

        std::array<double, 2> wVal{
            {std::log(mP / (1 - mP)),
            std::log((1 - mP) / mP)} };

        auto nan = std::nan("");
        std::fill(mR.begin(), mR.end(), nan);
        std::fill(mM.begin(), mM.end(), nan);

        // #1
        for (u64 i = 0; i < n; ++i)
        {
            assert(codeword[i] < 2);
            mW[i] = wVal[codeword[i]];

            for (auto j : mH.mCols[i])
            {
                mR(j, i) = mW[i];
            }
        }

        std::vector<u8> c(n);
        std::vector<double> rr; rr.reserve(100);
        for (u64 ii = 0; ii < maxIter; ii++)
        {
            // #2
            for (u64 j = 0; j < m; ++j)
            {
                rr.resize(mH.mRows[j].size());
                for (u64 i : mH.mRows[j])
                {
                    // \Pi_{k in Nj \ {i} }  (r_k^j + 1)/(r_k^j - 1)
                    double v = std::numeric_limits<double>::max();
                    double s = 1;

                    for (u64 k : mH.mRows[j])
                    {
                        if (k != i)
                        {
                            assert(mR(j, k) != nan);

                            v = std::min(v,std::abs(mR(j,k)));

                            s *= sgn(mR(j, k));
                        }
                    }

                    // m_j^i 
                    mM(j, i) = s * v;
                }
            }

            // i indexes a column, [1,...,n]
            for (u64 i = 0; i < n; ++i)
            {
                // j indexes a row, [1,...,m]
                for (u64 j : mH.mCols[i])
                {
                    // r_i^j = w_i * Pi_{k in Ni \ {j} } m_k^i
                    mR(j, i) = mW[i];

                    // j indexes a row, [1,...,m]
                    for (u64 k : mH.mCols[i])
                    {
                        if (k != j)
                        {
                            //auto mr = mM.rows();
                            //assert(j < mR.rows());
                            //assert(i < mR.cols());
                            //assert(i < mM.rows());
                            //assert(k < mM.cols());
                            assert(mM(k, i) != nan);

                            mR(j, i) += mM(k, i);
                        }
                    }
                }
            }

            // i indexes a column, [1,...,n]
            for (u64 i = 0; i < n; ++i)
            {
                //log L(ci | wi, m^i)
                double L = mW[i];

                // k indexes a row, [1,...,m]
                for (u64 k : mH.mCols[i])
                {
                    assert(mM(k, i) != nan);
                    L += mM(k, i);
                }

                c[i] = (L >= 0) ? 0 : 1;
            }

            if (check(c))
            {
                c.resize(n - m);
                return c;
            }
        }

        return {};
    }

    bool LdpcDecoder::check(const span<u8>& data) {

        bool isCW = true;
        // j indexes a row, [1,...,m]
        for (u64 j = 0; j < mH.rows(); ++j)
        {
            u8 sum = 0;

            // i indexes a column, [1,...,n]
            for (u64 i : mH.mRows[j])
            {
                sum ^= data[i];
            }

            if (sum)
            {
                return false;
            }
        }
        return true;

        //for (int i = 0; i < mH.rows(); i++) {
        //    auto check_node = check_to_data_id[i];
        //    bool res = false;
        //    for (int j = 0; j < check_degree[i]; j++)
        //        res ^= data[check_node[j]];
        //    if (res)return false;
        //}
        //return true;
    }


    //bool LdpcDecoder::decode2(span<u8> data, u64 iterations)
    //{
    //    auto code_len = mH.cols();
    //    auto msg_len = mK;
    //    //first: initialze
    //    //bool* data_nodes_bin = (bool*)data_nodes;
    //    //bool* messages_bin = (bool*)messages;
    //    std::vector<u8> data_nodes_bin(code_len);
    //    std::vector<u8> messages_bin(msg_len);
    //    auto& data_to_check = mH.mRows;
    //    auto& check_to_data = mH.mCols;

    //    std::vector<u64> data_degree(mH.rows()), check_degree(mH.cols());
    //    for (u64 i = 0; i < data_degree.size(); ++i)
    //        data_degree[i] = mH.mRows[i].size();
    //    for (u64 i = 0; i < check_degree.size(); ++i)
    //        check_degree[i] = mH.mCols[i].size();

    //    for (int i = 0; i < code_len; i++) {
    //        data_nodes_bin[i] = data[i];
    //        auto data_node = data_to_check[i];
    //        for (int j = 0; j < data_degree[i]; j++)
    //            messages_bin[data_node[j]] = data_nodes_bin[i];
    //    }
    //    //second: bp
    //    for (int iter = 0; iter < iterations; iter++) {
    //        for (int i = 0; i < code_len - msg_len; i++) {
    //            bool msg_sum = false;
    //            auto check_node = check_to_data[i];
    //            for (int j = 0; j < check_degree[i]; j++) {
    //                msg_sum ^= messages_bin[check_node[j]];
    //            }
    //            for (int j = 0; j < check_degree[i]; j++) {
    //                messages_bin[check_node[j]] = msg_sum ^ messages_bin[check_node[j]];
    //            }
    //        }
    //        for (int i = 0; i < code_len; i++) {
    //            auto data_node = data_to_check[i];
    //            int pos = 0, neg = 0;
    //            for (int j = 0; j < data_degree[i]; j++) {
    //                if (messages_bin[data_node[j]])pos++;
    //                else neg++;
    //            }

    //            int t = pos - neg + (data_nodes_bin[i] ? 1 : -1);
    //            for (int j = 0; j < data_degree[i]; j++) {
    //                int tt = messages_bin[data_node[j]] ? (t - 1) : (t + 1);
    //                messages_bin[data_node[j]] = (tt == 0) ? data_nodes_bin[i] : (tt > 0);
    //            }
    //            data[i] = t == 0 ? data_nodes_bin[i] : t > 0;
    //        }
    //        //check
    //        if (check(data)) {
    //            return true;
    //        }
    //    }

    //    return false;
    //}

    void tests::LdpcDecode_pb_test(const CLP& cmd)
    {
        u64 rows = cmd.getOr("r", 1000);
        u64 cols = rows * cmd.getOr("e", 2.0);
        u64 colWeight = cmd.getOr("cw", 4);
        u64 dWeight = cmd.getOr("dw", 3);
        u64 gap = cmd.getOr("g", 12);

        auto k = cols - rows;
        assert(gap >= dWeight);

        SparseMtx H;
        LdpcEncoder E;
        LdpcDecoder D;

        for (u64 i = 0; i < 40; ++i)
        {
            PRNG prng(block(i, 1));
            bool b = true;
            u64 tries = 0;
            while (b)
            {
                H = sampleTriangularBand(rows, cols, colWeight, gap, dWeight, prng);
                // H = sampleTriangular(rows, cols, colWeight, gap, prng);
                b = !E.init(H, gap);

                ++tries;
            }

            std::cout << "samples " << tries << std::endl;

            u64 d;
            
            if(cols < 35)
                d = minDist(H.dense(), false).second.size();

            LDPC_bp_decoder DD(cols, rows);
            DD.init(H);
            D.init(H);
            std::vector<u8> m(k), code(cols);

            for (auto& mm : m)
                mm = prng.getBit();

            E.encode(code, m);
            auto ease = 1ull;


            u64 logBP = 0;
            u64 BP = 0;
            u64 minSum = 0;
            while (true)
            {
                auto c = code;
                for (u64 j = 0; j < ease; ++j)
                {
                    c[j] ^= 1;
                }

                auto m2 = D.logbpDecode(c);
                if (m2 != m)
                    break;
                //if (m2 != m && !logBP)
                //    logBP = ease;

                //auto m3 = D.bpDecode(c);
                //if (m3 != m && !BP)
                //    BP = ease;

                //auto m4 = D.minSumDecode(c);
                //if (m4 != m && !minSum)
                //    minSum = ease;

                //if (logBP && BP && minSum)
                //    break;

                ease = std::min<u64>(ease * 2, cols);
                std::cout << "ease " << ease << std::endl;
            }
            std::cout << "high " << ease << std::endl;

            auto low = ease / 2;
            auto high = ease;

            while (low + 1 < high)
            {
                auto mid = low + (high - low) / 2;
                std::cout << "mid " << mid << std::endl; 

                auto c = code;
                for (u64 j = 0; j < mid; ++j)
                {
                    c[j] ^= 1;
                }
                auto m2 = D.logbpDecode(c);
                if (m2 != m)
                    high = mid;
                else
                    low = mid;
            }

            logBP = low;
            //assert(logBP);
            //assert(BP);
            //assert(minSum);

            std::cout <<i << " " << d << ": "<< logBP << " " << BP << " " << minSum << std::endl;
        }
        return;

    }

}