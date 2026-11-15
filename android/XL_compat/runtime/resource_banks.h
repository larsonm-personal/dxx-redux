#ifndef DXX_XL_RESOURCE_BANKS_H
#define DXX_XL_RESOURCE_BANKS_H

#include <memory>
#include <string>
#include <vector>

// Immutable native bank bytes and records, independent of installed paging,
// replacement pixels, mixer samples and registry pointers
class XlResourceBanks
{
  public:
	XlResourceBanks(std::vector<unsigned char> bitmaps, std::vector<unsigned char> sounds, int sample_rate);
	~XlResourceBanks();
	void bitmap(int id) const;
	void sound(int id) const;
	int bitmap_count() const;
	int sound_count() const;
	int sound_length(int id) const;
	int sample_rate() const;
	const std::string &bitmap_hash() const;
	const std::string &sound_hash() const;

  private:
	struct State;
	std::unique_ptr<State> state;
};

#endif
