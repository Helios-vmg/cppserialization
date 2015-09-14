public:
node(){}
node(std::uint64_t name, const std::string &data): name(name), data(data){}
void add_child(node *child){
	this->children.push_back(child);
}
std::string stringize() const;
