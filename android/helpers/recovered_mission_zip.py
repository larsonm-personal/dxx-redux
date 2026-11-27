"""Write a separate, reproducible local mission archive."""

import io
import zipfile


def write_mission_zip(output, members):
    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, "w") as archive:
        for filename, content in members:
            # Fixed metadata also makes repeated private builds comparable.
            info = zipfile.ZipInfo(filename)
            info.create_system = 3
            info.external_attr = 0o100644 << 16
            archive.writestr(info, content, compress_type=zipfile.ZIP_DEFLATED)
    with output.open("xb") as stream:
        try:
            stream.write(buffer.getbuffer())
            stream.flush()
        except BaseException:
            stream.close()
            output.unlink()
            raise
