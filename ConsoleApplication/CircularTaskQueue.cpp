#include "CircularTaskQueue.h"

#include "SharedMemory.cpp"

template<typename T>
CircularTaskQueue<T>::CircularTaskQueue()
{
	//header_->front = 0;
	//header_->rear = header_->front;

	//lock_ = (mt*)mmap(NULL, sizeof(*lock_), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
	//memset(lock_, 0x00, sizeof(*lock_));

	count_ = 0;
	length_ = 10;

	//queue_ = new Task[10];
}

template<typename T>
CircularTaskQueue<T>::~CircularTaskQueue()
{

}

template<typename T>
bool CircularTaskQueue<T>::put(const T task)
{
	lock_->lock();
	
	while (count_ == length_)
		pthread_cond_wait(&lock_->not_full, &lock_->mutex);

	enqueue(task);
	
	lock_->unlock();
	return true;
}

template<typename T>
void CircularTaskQueue<T>::enqueue(const T task)
{
	queue_[put_index] = task;

	if (++put_index == length_)
		put_index = 0;

	count_++;

	printf("enqueue task %d", task.serial_number);
	
	pthread_cond_signal(&lock_->not_empty);
}

template<typename T>
bool CircularTaskQueue<T>::take(T& task)
{
	lock_->lock();
	while (count_ == 0)
		pthread_cond_wait(&lock_->not_empty, &lock_->mutex);

	task = dequeue();

	printf("task B: %d\n", task.serial_number);

	lock_->unlock();
	return true;
}

template<typename T>
T CircularTaskQueue<T>::dequeue()
{
	T task = queue_[take_index];
	//queue_[take_index] = NULL;

	printf("take_index: %d\n", take_index);
	printf("task A: %d\n", task.serial_number);
	
	if (++take_index == length_)
		take_index = 0;

	count_--;
	pthread_cond_signal(&lock_->not_full);
	return task;
}

template<typename T>
atomic<size_t> CircularTaskQueue<T>::getTail()
{
	return header_->rear;
}

template<typename T>
atomic<size_t> CircularTaskQueue<T>::getHead()
{
	return header_->front;
}

template<typename T>
int CircularTaskQueue<T>::size()
{
	return header_->size;
}

template<typename T>
bool CircularTaskQueue<T>::isEmpty()
{
	return ((header_->rear + 1) % header_->size) == header_->front;
}

template<typename T>
bool CircularTaskQueue<T>::isFull()
{
	return header_->rear == header_->front;
}

struct mta
{
	int num;
	pthread_mutex_t mutex;
	pthread_mutexattr_t mutexattr;
	pthread_cond_t cond;
	pthread_condattr_t condattr;
};

struct header
{
	int length;
	int count;
	int put_index;
	int take_index;
};

int main(int argc, char * argv[])
{

	struct mta* mm;
	mm = (mta*)mmap(NULL, sizeof(*mm), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
	memset(mm, 0x00, sizeof(*mm));

	pthread_mutexattr_init(&mm->mutexattr);												// 初始化 mutex 属性
	pthread_mutexattr_setpshared(&mm->mutexattr, PTHREAD_PROCESS_SHARED);               // 修改属性为进程间共享
	pthread_mutex_init(&mm->mutex, &mm->mutexattr);

	pthread_condattr_init(&mm->condattr);
	pthread_condattr_setpshared(&mm->condattr, PTHREAD_PROCESS_SHARED);
	pthread_cond_init(&mm->cond, &mm->condattr);
	
	pid_t childPid = fork();

	if (childPid != 0)
	{
		printf("now we are in parent progress,pid=%d\n", (int)getpid());
		printf("now we are in parent progress,childPid = %d\n", childPid);

		SharedMemory<test>* sm = new SharedMemory<test>("/tmp", 66);
		SharedMemory<header>* sm2 = new SharedMemory<header>("/tmp", 67);

		printf("---------------save--------------\n");
		test t;
		sm->get_shm(sizeof(test)*10);
		header h;
		sm2->get_shm(sizeof(header));

		sleep(2);

		pthread_mutex_lock(&mm->mutex);
		printf("Parent Lock\n");
		
		test* ptr = sm->get_shm_ptr();

		ptr[1].age = 10;
		ptr[2].age = 12;

		header* ptr2 = sm2->get_shm_ptr();
		ptr2->count = 2;

		//ptr->age = 10;
		sm->save_shm(ptr);
		sm2->save_shm(ptr2);
		//delete sm;

		sleep(2);

		printf("Parent Cond signal A\n");
		pthread_cond_signal(&mm->cond);
		printf("Parent Cond signal B\n");

		pthread_mutex_unlock(&mm->mutex);
		printf("Parent unLock\n");
	}
	else
	{
		printf("now we are in child progress,pid = %d\n", (int)getpid());
		printf("now we are in child progress,parentPid = %d\n", (int)getppid());

		SharedMemory<test>* sm1 = new SharedMemory<test>("/tmp", 66);
		SharedMemory<header>* sm3 = new SharedMemory<header>("/tmp", 67);

		pthread_mutex_lock(&mm->mutex);
		printf("Child Lock\n");

		printf("Child Cond wait A\n");
		pthread_cond_wait(&mm->cond, &mm->mutex);
		printf("Child Cond wait B\n");


		//sleep(1);
		printf("---------------get--------------\n");
		test t;
		sm1->get_shm(sizeof(test) * 10);
		header h;
		sm3->get_shm(sizeof(header));

		test* ptr1 = sm1->get_shm_ptr();

		header* ptr2 = sm3->get_shm_ptr();

		printf("age:%d %d\n", ptr1[1].age, ptr1[2].age);

		printf("count: %d\n", ptr2->count);

		sm1->save_shm(ptr1);
		delete sm1;
		delete sm3;

		pthread_mutex_unlock(&mm->mutex);
		printf("Child unLock\n");
	}

	pthread_mutexattr_destroy(&mm->mutexattr);  // 销毁 mutex 属性对象
	pthread_mutex_destroy(&mm->mutex);          // 销毁 mutex 锁

	pthread_condattr_destroy(&mm->condattr);
	pthread_cond_destroy(&mm->cond);

	getchar();

	return 0;
}