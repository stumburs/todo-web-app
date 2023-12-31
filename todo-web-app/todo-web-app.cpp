#include "pch.h"

struct Task
{
	int id;
	std::string description;
	std::string time;
};

std::vector<Task> tasks;
std::mutex tasks_mutex;

int get_latest_task_id(std::string file_name)
{
	std::ifstream file(file_name);
	if (!file.is_open()) {
		std::cerr << "Error opening file: " << file_name << std::endl;
		return -1;
	}
	nlohmann::ordered_json j = nlohmann::ordered_json::parse(file);
	file.close();

	int last_id = 0;
	for (const auto& entry : j.items()) {
		int id = std::stoi(entry.key());
		last_id = std::max(last_id, id);
	}
	std::cout << "latest id = " << last_id << '\n';
	return last_id + 1;
}

int task_id_counter = get_latest_task_id("tasks.json");

void load_tasks_from_file(const std::string& file_name) {
	std::ifstream file(file_name);
	if (!file.is_open()) {
		std::cerr << "Error opening file: " << file_name << std::endl;
		return;
	}
	nlohmann::ordered_json j = nlohmann::ordered_json::parse(file);

	tasks.clear();

	for (const auto& entry : j.items()) {
		for (const auto& values : entry.value().items())
		{
			int id = std::stoi(values.key());
			std::string description = values.value().at("description");
			std::string time = values.value().at("time");
			tasks.push_back({ id, description, time });
		}
	}

	file.close();
}

void save_tasks_to_file(const std::string& file_name) {
	std::ofstream file(file_name);
	if (!file.is_open()) {
		std::cerr << "Error opening file: " << file_name << std::endl;
		return;
	}

	nlohmann::ordered_json j;

	for (const auto& task : tasks) {
		nlohmann::ordered_json j_task;
		j_task[std::to_string(task.id)]["description"] = task.description;
		j_task[std::to_string(task.id)]["time"] = task.time;
		j[task.id] = j_task;
	}
	file << j;
	file.close();
}

// Assuming 'time' is the std::time_t value obtained from the previous step
std::string format_time(const std::time_t& time) {
	struct tm time_info;
	localtime_s(&time_info, &time);

	std::stringstream ss;
	ss << std::put_time(&time_info, "%Y-%m-%d %H:%M:%S");
	return ss.str();
}

void add_task(const std::string& description) {
	std::lock_guard<std::mutex> lock(tasks_mutex);
	std::chrono::system_clock::time_point time_point = std::chrono::system_clock::now();
	std::string formatted_time = format_time(std::chrono::system_clock::to_time_t(time_point));
	tasks.push_back({ task_id_counter++, description, formatted_time });
	save_tasks_to_file("tasks.json"); // Save tasks to JSON file after adding
}

void delete_task(int task_id) {
	std::lock_guard<std::mutex> lock(tasks_mutex);
	tasks.erase(std::remove_if(tasks.begin(), tasks.end(), [task_id](const Task& task)
		{
			return task.id == task_id;
		}), tasks.end());
	save_tasks_to_file("tasks.json"); // Save tasks to JSON file after deletion
}

int main()
{
	load_tasks_from_file("tasks.json");
	crow::SimpleApp app;
	CROW_ROUTE(app, "/")([]()
		{
			return crow::mustache::load("index.html").render();
		});

	// Route to add a new task
	CROW_ROUTE(app, "/add_task").methods("POST"_method)
		([](const crow::request& req)
			{
				auto json = crow::json::load(req.body);
				if (!json) {
					return crow::response(400, "Invalid JSON format.");
				}

				if (!json.has("description")) {
					return crow::response(400, "Missing 'description' field.");
				}

				std::string description = json["description"].s();
				add_task(description);

				return crow::response(200, "Task added successfully.");
			});

	// Route to view all tasks
	CROW_ROUTE(app, "/view_tasks")
		([]()
			{
				std::lock_guard<std::mutex> lock(tasks_mutex);
				crow::json::wvalue json_tasks;
				for (const auto& task : tasks) {
					json_tasks[std::to_string(task.id)]["description"] = task.description;
					json_tasks[std::to_string(task.id)]["time"] = task.time;
				}

				return crow::response(200, json_tasks);
			});

	// Route to delete a task
	CROW_ROUTE(app, "/delete_task/<int>").methods("DELETE"_method)
		([](int task_id)
			{
				delete_task(task_id);
				return crow::response(200, "Task deleted successfully.");
			});


	app.port(5000).multithreaded().run();

	return 0;
}