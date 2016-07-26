//
// Created by cheyulin on 4/15/16.
//

#ifndef CODES_YCHE_CIS_ALGORITHM_H
#define CODES_YCHE_CIS_ALGORITHM_H

#include <boost/graph/adjacency_list.hpp>
#include <memory>
#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <vector>
#include <limits>
#include <iostream>

#define DOUBLE_ACCURACY 0.00001

using namespace std;
using namespace boost;

//Add User-Defined Tags For Boost Graph Library Usage
enum vertex_id_t {
    vertex_id
};
namespace boost {
    BOOST_INSTALL_PROPERTY(vertex, id);
}

namespace yche {

    //Iterative Scan Related Types
    using IndexType = unsigned long;
    //Now Replace the Map with Unordered Map to Improve Memory Access Efficiency
    using CommunityMemberSet = std::unordered_set<IndexType>;
    using CommunityMemberVec = std::vector<IndexType>;

    //Two Mutation Types in Iterative Scan
    enum class MutationType {
        add_neighbor,
        remove_member
    };

    //Now Replace the Map with Unordered Map to Improve Memory Access Efficiency
    struct MemberInfo;
    using MemberInfoMap = std::unordered_map<IndexType, unique_ptr<MemberInfo>>;

    //Cache the Previous Computation Results for a Single Vertex
    struct MemberInfo {
        MemberInfo(IndexType member_index) : member_index_(member_index), w_in_(0), w_out_(0) {}

        MemberInfo(const MemberInfo &member_info) {
            this->member_index_ = member_info.member_index_;
            this->w_in_ = member_info.w_in_;
            this->w_out_ = member_info.w_out_;
        }

        IndexType member_index_;
        double w_in_;
        double w_out_;

        void UpdateInfoForMutation(double edge_weight, MutationType mutation_type) {

        }
    };

    //Cache the Previous Computation Results for a Community
    struct CommunityInfo {
        CommunityInfo(double w_in, double w_out) : w_in_(w_in), w_out_(w_out) {}

        unique_ptr<CommunityMemberSet> members_;
        double w_in_;
        double w_out_;

        void UpdateInfoForMutation(const MemberInfo &member_info, MutationType mutation_type) {

        }
    };

    class Cis {
    public:
        //Graph Representation Related Types
        using EdgeProperties = property<edge_weight_t, double>;
        using VertexProperties = property<vertex_index_t, IndexType>;
        using Graph = adjacency_list<hash_setS, vecS, undirectedS, VertexProperties, EdgeProperties>;
        using Vertex = graph_traits<Graph>::vertex_descriptor;
        using Edge = graph_traits<Graph>::edge_descriptor;

        //Overlapping Community Results Related Types
        using OverlappingCommunityVec=vector<unique_ptr<CommunityMemberVec>>;

        //Start Implementation Interfaces For Parallelizer Traits
        unique_ptr<OverlappingCommunityVec> overlap_community_vec_;
        using BasicData = CommunityMemberSet;
        using MergeData = CommunityMemberVec;

        unique_ptr<vector<unique_ptr<BasicData>>> InitBasicComputationData();

        unique_ptr<MergeData> LocalComputation(unique_ptr<BasicData> seed_member_ptr);

        void MergeToGlobal(unique_ptr<MergeData> &result);

        //Start Implementation Interfaces For Reducer Traits
        using ReduceData = OverlappingCommunityVec;

        unique_ptr<ReduceData> WrapMergeDataToReduceData(unique_ptr<MergeData>& merge_data_ptr);

        function<bool(unique_ptr<ReduceData> &, unique_ptr<ReduceData> &)> CmpReduceData;

        function<unique_ptr<ReduceData>(unique_ptr<ReduceData>&,
                                        unique_ptr<ReduceData>& right_data_ptr)> ReduceComputation;

        [[deprecated("Replaced With Parallel Execution")]]
        unique_ptr<OverlappingCommunityVec> ExecuteCis();

        Cis(unique_ptr<Graph> graph_ptr, double lambda, map<int, int> &vertex_name_map) :
                lambda_(lambda), vertex_name_map_(vertex_name_map) {
            graph_ptr_ = std::move(graph_ptr);
            vertices_.clear();
            //Init Vertices
            property_map<Graph, vertex_index_t>::type vertex_index_map = boost::get(vertex_index, *graph_ptr_);
            for (auto vp = vertices(*graph_ptr_); vp.first != vp.second; ++vp.first) {
                Vertex vertex = *vp.first;
                vertices_.push_back(*vp.first);
            }

            overlap_community_vec_ = make_unique<OverlappingCommunityVec>();

            CmpReduceData = [](unique_ptr<ReduceData> &left, unique_ptr<ReduceData> &right) -> bool {
                auto cmp = [](auto &tmp_left, auto &tmp_right) -> bool {
                    return tmp_left->size() < tmp_right->size();
                };
                auto iter1 = max_element(left->begin(), left->end(), cmp);
                auto iter2 = max_element(left->begin(), left->end(), cmp);
                return (*iter1)->size() > (*iter2)->size();
            };

            ReduceComputation = [this](
                    unique_ptr<ReduceData>& left_data_ptr,
                    unique_ptr<ReduceData>& right_data_ptr) -> unique_ptr<ReduceData> {
                for (auto &right_merge_data:*right_data_ptr) {
                    MergeToCommunityCollection(left_data_ptr, right_merge_data);
                }
                return std::move(left_data_ptr);
            };
        }

    private:

        map<int, int> vertex_name_map_;
        unique_ptr<Graph> graph_ptr_;
        vector<Vertex> vertices_;

        double lambda_;

        double CalculateDensity(const IndexType &size, const double &w_in, const double &w_out, const double &lambda);

        unique_ptr<CommunityInfo> SplitAndChooseBestConnectedComponent(unique_ptr<CommunityMemberSet> &community_ptr);

        void UpdateMembersNeighborsCommunityInfo(const unique_ptr<Graph> &graph_ptr, const Vertex &mutate_vertex,
                                                 unique_ptr<CommunityInfo> &community_info_ptr,
                                                 CommunityMemberSet &members,
                                                 CommunityMemberSet &neighbors, const MutationType &mutation_type);

        void InitializationForSeedExpansion(const unique_ptr<CommunityMemberSet> &seed_member_ptr,
                                            unique_ptr<CommunityInfo> &community_info_ptr, MemberInfoMap &members,
                                            MemberInfoMap &neighbors, CommunityMemberSet &to_computed_neighbors,
                                            property_map<Graph, vertex_index_t>::type &vertex_index_map,
                                            property_map<Graph, edge_weight_t>::type &edge_weight_map);

        unique_ptr<CommunityMemberVec> ExpandSeed(unique_ptr<CommunityMemberSet> &seed_member_ptr);

        double GetTwoCommunitiesCoverRate(unique_ptr<CommunityMemberVec> &left_community,
                                          unique_ptr<CommunityMemberVec> &right_community);

        unique_ptr<CommunityMemberVec> MergeTwoCommunities(unique_ptr<CommunityMemberVec> &left_community,
                                                           unique_ptr<CommunityMemberVec> &right_community);

        void MergeToCommunityCollection(decltype(overlap_community_vec_) &community_collection,
                                        unique_ptr<MergeData> &result);
    };
}

#endif //CODES_YCHE_CIS_ALGORITHM_H