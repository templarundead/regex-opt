#include <vector>
#include <list>
#include <bitset>
#include <ostream>

///////////////////////

typedef std::bitset<256> regexopt_charset;
typedef std::vector<struct regexopt_item> regexopt_sequence;
typedef std::list<regexopt_sequence> regexopt_choices;

const regexopt_choices RegexOptParse(const std::string& s, unsigned& pos);

void DumpRegexOptTree(std::ostream& out, const regexopt_choices& tree);

//////////////////////

struct regexopt_item
{
    regexopt_choices* tree;
    regexopt_charset ch; // used if tree is 0.
    
    unsigned min;
    unsigned max;
    bool greedy;
    // bool bol;
    // bool eol;
    
    regexopt_item(): tree(0),ch(),min(1),max(1),greedy(true)
    {
    }
    
    ~regexopt_item()
    {
        //if(tree) delete tree;
    }
    
    bool is_equal(const regexopt_item& b) const;
    bool is_subset_of(const regexopt_item& b) const; // TODO: implement
    
    bool is_combinable(const regexopt_item& b) const;
    
    bool operator!=(const regexopt_item& b) const { return !operator==(b); }
    bool operator==(const regexopt_item& b) const;
    
    void Optimize();
//private:
//    void operator= (const regexopt_item);
//    item(const regexopt_item);
};


/* Note: This lib is not yet suitable for production-use.
 * The "tree" pointer in the regexopt_item structure is not properly
 * deallocated. In other words, there's a memory leak.
 *
 * It should be solved with an autoptr class or something like that.
 */
