//
// Created by cheyulin on 2/24/16.
//

#include "damon_algorithm.h"

namespace yche {

    unique_ptr<Daemon::SubGraph> Daemon::ExtractEgoMinusEgo(Daemon::Vertex ego_vertex) {
        unique_ptr<SubGraph> ego_net_ptr = make_unique<SubGraph>();
        auto origin_sub_index_map = map<int, int>();

        property_map<SubGraph, vertex_id_t>::type sub_vertex_id_map =
                get(vertex_id, *ego_net_ptr);
        property_map<SubGraph, vertex_index_t>::type sub_vertex_index_map =
                get(vertex_index, *ego_net_ptr);
        property_map<SubGraph, vertex_weight_t>::type sub_vertex_weight_map =
                get(vertex_weight, *ego_net_ptr);
        property_map<SubGraph, vertex_label_t>::type sub_vertex_label_map =
                get(vertex_label, *ego_net_ptr);

        property_map<Graph, vertex_index_t>::type vertex_index_map =
                get(vertex_index, *graph_ptr_);
        property_map<Graph, vertex_weight_t>::type vertex_weight_map =
                get(vertex_weight, *graph_ptr_);

        vector<SubGraphVertex> sub_vertices;
        //Add SubVertices
        for (auto vp = adjacent_vertices(ego_vertex, *graph_ptr_); vp.first != vp.second; ++vp.first) {
            //Add Vertex
            graph_traits<SubGraph>::vertex_descriptor sub_vertex = add_vertex(*ego_net_ptr);
            sub_vertices.push_back(sub_vertex);

            //Add Vertex Property
            auto graph_vertex_index = vertex_index_map[*vp.first];

            sub_vertex_id_map[sub_vertex] = graph_vertex_index;
            sub_vertex_weight_map[sub_vertex] = vertex_weight_map[*vp.first];

            sub_vertex_label_map[sub_vertex][0] = sub_vertex_id_map[sub_vertex];
            sub_vertex_label_map[sub_vertex][1] = 0;

            //Add Indexing Info
            origin_sub_index_map.insert(make_pair(graph_vertex_index, sub_vertex_index_map[sub_vertex]));
        }

        //Add Edges
        for (auto vp = adjacent_vertices(ego_vertex, *graph_ptr_); vp.first != vp.second; ++vp.first) {
            auto source_sub_vertex_index = origin_sub_index_map[vertex_index_map[*vp.first]];
            auto source_sub_vertex = sub_vertices[source_sub_vertex_index];
            for (auto vp_inner = adjacent_vertices(ego_vertex, *graph_ptr_);
                 vp_inner.first != vp_inner.second; ++vp_inner.first) {
                bool is_edge_exists = edge(*vp.first, *vp_inner.first, *graph_ptr_).second;
                if (is_edge_exists) {
                    auto end_sub_vertex_index = origin_sub_index_map[vertex_index_map[*vp_inner.first]];
                    auto end_vertex_ptr = sub_vertices[end_sub_vertex_index];
                    add_edge(source_sub_vertex, end_vertex_ptr, *ego_net_ptr);
                }
            }
        }
        return std::move(ego_net_ptr);
    }

    Daemon::CommunityVecPtr Daemon::LabelPropagationOnSubGraph
            (unique_ptr<Daemon::SubGraph> sub_graph_ptr, Daemon::Vertex ego_vertex) {

        int iteration_num = 0;

        property_map<SubGraph, vertex_id_t>::type sub_vertex_id_map =
                get(vertex_id, *sub_graph_ptr);
        property_map<SubGraph, vertex_index_t>::type sub_vertex_index_map =
                get(vertex_index, *sub_graph_ptr);
        property_map<SubGraph, vertex_weight_t>::type sub_vertex_weight_map =
                get(vertex_weight, *sub_graph_ptr);
        property_map<SubGraph, vertex_label_t>::type sub_vertex_label_map =
                get(vertex_label, *sub_graph_ptr);

        while (iteration_num < max_iteration_num_) {
            auto curr_index_indicator = (iteration_num + 1) % 2;
            auto last_index_indicator = iteration_num % 2;
            //Each V Do One Propagation
            for (auto vp = vertices(*sub_graph_ptr); vp.first != vp.second; ++vp.first) {
                auto label_weight_map = map<int, double>();
                //Label Propagation
                for (auto vp_inner = adjacent_vertices(*vp.first, *sub_graph_ptr);
                     vp_inner.first != vp_inner.second; ++vp_inner.first) {
                    auto neighbor_vertex = *vp_inner.first;
                    auto neighbor_vertex_label = sub_vertex_label_map[neighbor_vertex][last_index_indicator];
                    auto neighbor_vertex_weight = sub_vertex_weight_map[neighbor_vertex];

                    auto my_iterator = label_weight_map.find(neighbor_vertex_label);
                    if (my_iterator == label_weight_map.end()) {
                        label_weight_map.insert(make_pair(neighbor_vertex_label, neighbor_vertex_weight));
                    }
                    else {
                        label_weight_map[neighbor_vertex_label] += neighbor_vertex_weight;
                    }
                }

                //Find Maximum Vote
                auto candidate_label_vec = vector<int>();
                auto max_val = 0;
                cout << "label_weight_map:" << label_weight_map.size() << endl;
                auto current_vertex = *vp.first;
                if (label_weight_map.size() == 0) {
                    cout << "!!!" << endl;
                    sub_vertex_label_map[current_vertex][curr_index_indicator] = sub_vertex_label_map[current_vertex][last_index_indicator];
                }
                else {
                    for (auto label_to_weight_pair:label_weight_map) {
                        auto label_weight = label_to_weight_pair.second;
                        if (label_weight > max_val) {
                            candidate_label_vec.clear();
                            max_val = label_weight;
                        }
                        if (label_weight >= max_val) {
                            candidate_label_vec.push_back(label_to_weight_pair.first);
                        }
                    }
                    srand(time(nullptr));
                    auto choice_index = rand() % candidate_label_vec.size();

                    sub_vertex_label_map[current_vertex][curr_index_indicator] = candidate_label_vec[choice_index];
                }

                //Update Label

            }

            iteration_num++;
        }

        //Construct Set of Set, i.e. Comms
        property_map<Graph, vertex_index_t>::type vertex_index_map =
                get(vertex_index, *graph_ptr_);
        map<int, CommunityPtr> label_indices_map;
        auto curr_index_indicator = (iteration_num + 1) % 2;

        for (auto vp = vertices(*sub_graph_ptr); vp.first != vp.second; ++vp.first) {
            auto sub_vertex = *vp.first;
            auto v_label = sub_vertex_label_map[sub_vertex][curr_index_indicator];
            if (label_indices_map.find(v_label) == label_indices_map.end()) {
                CommunityPtr community = make_unique<set<int>>();
                label_indices_map.insert(make_pair(v_label, std::move(community)));
            }

            label_indices_map[v_label]->insert(sub_vertex_id_map[sub_vertex]);
        }

        CommunityVecPtr cooms_vec_ptr = make_unique<vector<CommunityPtr>>();
        for (auto iter = label_indices_map.begin(); iter != label_indices_map.end(); ++iter) {
            //Add Ego Vertex
            iter->second->insert(vertex_index_map[ego_vertex]);
            cooms_vec_ptr->push_back(std::move(iter->second));
        }
        return std::move(cooms_vec_ptr);
    }

    Daemon::CommunityPtr Daemon::MergeTwoCommunities(CommunityPtr left_community, CommunityPtr right_community) {
        vector<int> union_set(left_community->size() + right_community->size());
        auto iter_end = set_union(left_community->begin(), left_community->end(), right_community->begin(),
                                  right_community->end(), union_set.begin());
        union_set.resize(iter_end - union_set.begin());
        CommunityPtr union_community = make_unique<set<int>>();
        for (auto iter = union_set.begin(); iter != union_set.end(); ++iter) {
            union_community->insert(*iter);
        }
        return std::move(union_community);
    }

    pair<double, pair<Daemon::CommunityPtr, Daemon::CommunityPtr>> Daemon::GetTwoCommunitiesCoverRate(
            CommunityPtr left_community, CommunityPtr right_community) {
        vector<int> intersect_set(left_community->size() + right_community->size());
        auto iter_end = set_intersection(left_community->begin(), left_community->end(), right_community->begin(),
                                         right_community->end(), intersect_set.begin());
        intersect_set.resize(iter_end - intersect_set.begin());
        return make_pair(
                static_cast<double>(intersect_set.size()) / min(left_community->size(), right_community->size()),
                make_pair(std::move(left_community), std::move(right_community)));
    }

    void Daemon::ExecuteDaemon() {
        //Clear Former Results
        overlap_community_vec_ = make_unique<vector<CommunityPtr>>();
        for (auto vp = vertices(*graph_ptr_); vp.first != vp.second; ++vp.first) {
            auto ego_vertex = *vp.first;
            auto sub_graph_ptr = ExtractEgoMinusEgo(ego_vertex);
            auto community_vec_ptr = std::move(LabelPropagationOnSubGraph(std::move(sub_graph_ptr), ego_vertex));
            for (auto iter = overlap_community_vec_->begin(); iter != overlap_community_vec_->end(); ++iter) {
                for (auto iter_inner = community_vec_ptr->begin();
                     iter_inner != community_vec_ptr->end(); ++iter_inner) {
                    auto cover_rate_result = GetTwoCommunitiesCoverRate(std::move(*iter), std::move(*iter_inner));
                    *iter = std::move(cover_rate_result.second.first);
                    *iter_inner = std::move(cover_rate_result.second.second);
                    if (cover_rate_result.first > epsilon_) {
                        *iter = MergeTwoCommunities(std::move(*iter), std::move(*iter_inner));
                    }
                    else if ((*iter_inner)->size() > min_community_size_)
                        community_vec_ptr->push_back(std::move(*iter_inner));

                }
            }
        }
    }


}