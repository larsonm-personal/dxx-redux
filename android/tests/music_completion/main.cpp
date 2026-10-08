extern "C" int run_music_completion(const char *trace_path);

int main(int argc, char **argv)
{
	return argc == 2 ? run_music_completion(argv[1]) : 2;
}
