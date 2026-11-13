#ifndef DXX_XL_PREPARED_HAM_H
#define DXX_XL_PREPARED_HAM_H

#include <memory>
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
	~XlPreparedHam();
	XlPreparedHam(XlPreparedHam &&) noexcept;
	XlPreparedHam &operator=(XlPreparedHam &&) noexcept;
	XlPreparedHam(const XlPreparedHam &) = delete;
	XlPreparedHam &operator=(const XlPreparedHam &) = delete;
	const bm_ham_destination &tables() const;

  private:
	struct State;
	std::unique_ptr<State> state;
};
#endif
