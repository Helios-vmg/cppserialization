public:
graph(){}
void generate(unsigned seed);
void stringize(std::ostream &) const;
std::set<node *> get_all_nodes(size_t known_max = std::numeric_limits<size_t>::max()) const;
std::unordered_set<node *> get_all_nodes_unordered(size_t known_max = std::numeric_limits<size_t>::max()) const;
decltype(graph::node_count) get_node_count() const {
	return this->node_count;
}
private:
template <typename T>
void get_all_nodes_generic(T &dst, size_t known_max = std::numeric_limits<size_t>::max()) const;
