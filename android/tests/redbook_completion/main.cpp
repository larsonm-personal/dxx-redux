extern "C" int run_redbook_completion(const char *trace_path);

int main(int argc, char **argv)
{
	return argc == 2 ? run_redbook_completion(argv[1]) : 2;
}
