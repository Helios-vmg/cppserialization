public:
node(){}
node(std::uint64_t name, const std::string &data): name(name), data(data){}
void add_child(node *child){
	this->children.push_back(child);
}
void stringize(std::ostream &) const;
void get_children(std::vector<node *> &dst) const{
	for (auto c : this->children)
		dst.push_back(c);
}
std::uint64_t get_name() const{
	return this->name;
}
