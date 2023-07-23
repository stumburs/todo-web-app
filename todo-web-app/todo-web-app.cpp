#include "pch.h"

struct Task
{
	int id;
	std::string description;
};

std::vector<Task> tasks;
std::mutex tasks_mutex;

int task_id_counter = 0;

void add_task(const std::string& description) {
	std::lock_guard<std::mutex> lock(tasks_mutex);
	tasks.push_back({ task_id_counter++, description });
}

void delete_task(int task_id) {
	std::lock_guard<std::mutex> lock(tasks_mutex);
	tasks.erase(std::remove_if(tasks.begin(), tasks.end(), [task_id](const Task& task)
		{
			return task.id == task_id;
		}), tasks.end());
}

int main()
{
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
					json_tasks[std::to_string(task.id)] = task.description;
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