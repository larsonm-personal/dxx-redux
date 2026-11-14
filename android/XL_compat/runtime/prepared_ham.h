#ifndef DXX_XL_PREPARED_HAM_H
#define DXX_XL_PREPARED_HAM_H

#include <memory>
#include <string>
#include <vector>
extern "C" {
#include "bm_ham.h"
#include "mission_prepare.h"
}

// Independent target definitions decoded through the native HAM reader
// Extensions, resource banks and world publication remain separate owners
class XlPreparedHam
{
  public:
	explicit XlPreparedHam(mission_preparation *mission, const char *member);
	// The caller establishes source ownership; the file remains caller-owned
	explicit XlPreparedHam(PHYSFS_file *file);
	explicit XlPreparedHam(std::vector<unsigned char> bytes);
	static XlPreparedHam extra(PHYSFS_file *file);
	int extra_bitmap_count() const;
	int extra_pointer_count() const;
	~XlPreparedHam();
	XlPreparedHam(XlPreparedHam &&) noexcept;
	XlPreparedHam &operator=(XlPreparedHam &&) noexcept;
	XlPreparedHam(const XlPreparedHam &) = delete;
	XlPreparedHam &operator=(const XlPreparedHam &) = delete;
	const bm_ham_destination &tables() const;
	std::string source_hash() const;
	void install();

  private:
	XlPreparedHam();
	struct State;
	std::unique_ptr<State> state;
};
#endif
