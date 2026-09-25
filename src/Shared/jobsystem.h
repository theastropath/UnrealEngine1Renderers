/*=============================================================================
	jobsystem.h: a worker pool for parallel geometry building. Header only.

	A job writes the span its index maps to, visible once the dispatch returns.
	No graphics API call from a job.
=============================================================================*/

#ifndef UTGLR_JOBSYSTEM_H
#define UTGLR_JOBSYSTEM_H

//Core's clock() macro collides with the standard headers below.
#pragma push_macro("clock")
#undef clock
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#ifdef JS_DEBUG_WATCHDOG
#include <chrono>
#endif
#pragma pop_macro("clock")

#include <float.h>
#include <malloc.h>
#include <new>
#include <stddef.h>
#include <xmmintrin.h> //_mm_pause
#ifdef JS_DEBUG_WATCHDOG
#include <stdio.h>
#include <stdlib.h>
#endif


#define JS_CACHE_LINE 64

#define JS_MIN_JOB_BYTES 4096

#define JS_DISPATCH_SPIN 4096

/**
\param Context Whatever the caller passed to Dispatch.
\param JobIndex 0 .. JobCount-1, claimed by whoever gets there first.
*/
typedef void (*FJobFunc)(void *Context, int JobIndex);


//The renderers compile under /Zp4 for the game's ABI, which packs the primitives below
//to 4 bytes. The 64 bit ticket then loses its lock-free path and a claim can read torn.
#pragma pack(push, 8)


class FJobSystem {
public:
	FJobSystem() :
		m_fpuControlWord(0),
		m_pThreads(NULL),
		m_numWorkers(0) {
		m_pFunc.store(NULL, std::memory_order_relaxed);
		m_pContext.store(NULL, std::memory_order_relaxed);
		m_jobCount.store(0, std::memory_order_relaxed);
		m_generation.store(0, std::memory_order_relaxed);
		m_fpuGeneration.store(0, std::memory_order_relaxed);
		m_stop.store(false, std::memory_order_relaxed);
		m_faulted.store(false, std::memory_order_relaxed);
		m_ticket.store(0, std::memory_order_relaxed);
		m_remaining.store(0, std::memory_order_relaxed);
	}

	~FJobSystem() { Stop(); }

	/**
	\param requestedThreads Threads that will run jobs, the caller included. 0 asks for one per
		logical core. 1 keeps everything on the caller, which separates a deferred path bug
		from a threading one.
	*/
	void Start(int requestedThreads) {
		Stop();

		//Copied to the workers, so read it from whoever is starting the pool
		CaptureFpuControlWord();

		int total = requestedThreads;
		if (total <= 0) {
			total = (int)std::thread::hardware_concurrency() - 1;
			if (total <= 0) {
				total = 1;
			}
		}
		//Past 16, one frame's geometry stops dividing into jobs worth having.
		if (total > 16) {
			total = 16;
		}

		const int workers = total - 1;
		if (workers <= 0) {
			return;
		}

		m_stop.store(false, std::memory_order_relaxed);
		m_generation.store(0, std::memory_order_relaxed);
		m_ticket.store(0, std::memory_order_relaxed);

		m_pThreads = (std::thread *)_aligned_malloc(sizeof(std::thread) * workers, JS_CACHE_LINE);
		if (m_pThreads == NULL) {
			return;
		}

		for (int i = 0; i < workers; i++) {
			try {
				new (&m_pThreads[i]) std::thread(&FJobSystem::WorkerMain, this);
			} catch (...) {
				//A pool that is narrower than asked for still works
				break;
			}
			m_numWorkers = i + 1;
		}
	}

	void Stop() {
		if (m_pThreads != NULL) {
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				m_stop.store(true, std::memory_order_relaxed);
				m_generation.fetch_add(1, std::memory_order_release);
			}
			m_wake.notify_all();

			for (int i = 0; i < m_numWorkers; i++) {
				if (m_pThreads[i].joinable()) {
					m_pThreads[i].join();
				}
				m_pThreads[i].~thread();
			}
			_aligned_free(m_pThreads);
			m_pThreads = NULL;
		}
		m_numWorkers = 0;
		m_stop.store(false, std::memory_order_relaxed);
	}

	inline int NumParticipants() const { return m_numWorkers + 1; }

	/** \return false if a job faulted, the output then being incomplete. */
	bool Dispatch(int jobCount, FJobFunc pFunc, void *pContext) {
		if (jobCount <= 0) {
			return true;
		}

		m_faulted.store(false, std::memory_order_relaxed);

		if ((m_numWorkers == 0) || (jobCount == 1)) {
			for (int i = 0; i < jobCount; i++) {
				RunJob(pFunc, pContext, i);
			}
			return !m_faulted.load(std::memory_order_relaxed);
		}

		if (CaptureFpuControlWord()) {
			m_fpuGeneration.fetch_add(1, std::memory_order_relaxed);
		}

		const unsigned int generation = m_generation.load(std::memory_order_relaxed) + 1;

		//Ticket first: retiring the previous generation before the description is written
		//makes a straggler still inside it leave, where it could otherwise claim an index
		//twice. The fence pins those stores.
		m_ticket.store((unsigned long long)generation << 32, std::memory_order_release);
		std::atomic_thread_fence(std::memory_order_seq_cst);

		m_pFunc.store(pFunc, std::memory_order_relaxed);
		m_pContext.store(pContext, std::memory_order_relaxed);
		m_jobCount.store(jobCount, std::memory_order_relaxed);
		m_remaining.store(jobCount, std::memory_order_relaxed);

		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_generation.store(generation, std::memory_order_release);
		}
		m_wake.notify_all();

		RunClaimedJobs(generation);

		for (int spin = 0; spin < JS_DISPATCH_SPIN; spin++) {
			if (m_remaining.load(std::memory_order_acquire) == 0) {
				break;
			}
			_mm_pause();
		}

		if (m_remaining.load(std::memory_order_acquire) != 0) {
			std::unique_lock<std::mutex> lock(m_doneMutex);
			while (m_remaining.load(std::memory_order_acquire) != 0) {
#ifdef JS_DEBUG_WATCHDOG
				if (m_done.wait_for(lock, std::chrono::seconds(10)) == std::cv_status::timeout) {
					const unsigned long long t = m_ticket.load(std::memory_order_acquire);
					fprintf(stderr, "JS STUCK: dispatchGen=%u ticketGen=%u ticketIdx=%u jobCount=%d remaining=%d workers=%d poolGen=%u\n",
						generation, (unsigned int)(t >> 32), (unsigned int)t,
						m_jobCount.load(std::memory_order_relaxed), m_remaining.load(std::memory_order_relaxed),
						m_numWorkers, m_generation.load(std::memory_order_relaxed));
					fflush(stderr);
					_exit(3);
				}
#else
				m_done.wait(lock);
#endif
			}
		}

		return !m_faulted.load(std::memory_order_relaxed);
	}

private:
	FJobSystem(const FJobSystem &);
	FJobSystem &operator=(const FJobSystem &);

	/**
	Precision and rounding are per thread, and the device keeps what the game left set.
	A worker computing under different ones seams a surface split across two jobs.
	Masks stay masked: a trap kills the process.
	*/
	bool CaptureFpuControlWord() {
		unsigned int cw = 0;
		if (_controlfp_s(&cw, 0, 0) != 0) {
			return false;
		}
		cw &= (_MCW_PC | _MCW_RC | _MCW_DN);
		if (cw == m_fpuControlWord) {
			return false;
		}
		m_fpuControlWord = cw;
		return true;
	}

	inline void ApplyFpuControlWord() const {
		unsigned int old = 0;
		_controlfp_s(&old, m_fpuControlWord, _MCW_PC | _MCW_RC | _MCW_DN);
	}

	void RunJob(FJobFunc pFunc, void *pContext, int jobIndex) {
		try {
			pFunc(pContext, jobIndex);
		} catch (...) {
			m_faulted.store(true, std::memory_order_relaxed);
		}
	}

	/**
	\param myGeneration The dispatch this thread joined; a straggler from the previous one sees
		a generation it does not own and leaves. Hence compare-exchange: a fetch-add would take
		the index before the mismatch was noticed, and that index's job never run.
	*/
	void RunClaimedJobs(unsigned int myGeneration) {
		for (;;) {
			unsigned long long ticket = m_ticket.load(std::memory_order_acquire);
			if ((unsigned int)(ticket >> 32) != myGeneration) {
				break; //Another dispatch's
			}
			const int jobIndex = (int)(unsigned int)ticket;
			if (jobIndex >= m_jobCount.load(std::memory_order_relaxed)) {
				break;
			}
			if (!m_ticket.compare_exchange_weak(ticket, ticket + 1,
					std::memory_order_acq_rel, std::memory_order_acquire)) {
				continue; //Taken, or the dispatch moved on
			}
			RunJob(m_pFunc.load(std::memory_order_relaxed),
				m_pContext.load(std::memory_order_relaxed), jobIndex);
			//Release: this job's writes must reach whoever sees zero
			if (m_remaining.fetch_sub(1, std::memory_order_release) == 1) {
				//The mutex closes the waiter's test-then-wait gap.
				{
					std::lock_guard<std::mutex> lock(m_doneMutex);
				}
				m_done.notify_one();
			}
		}
	}

	void WorkerMain() {
		unsigned int localGeneration = 0;
		unsigned int localFpuGeneration = (unsigned int)-1;

		for (;;) {
			bool woke = false;
			for (int spin = 0; spin < 256; spin++) {
				if (m_generation.load(std::memory_order_acquire) != localGeneration) {
					woke = true;
					break;
				}
				_mm_pause();
			}

			if (!woke) {
				std::unique_lock<std::mutex> lock(m_mutex);
				while (m_generation.load(std::memory_order_acquire) == localGeneration) {
					m_wake.wait(lock);
				}
			}

			localGeneration = m_generation.load(std::memory_order_acquire);

			if (m_stop.load(std::memory_order_relaxed)) {
				return;
			}

			const unsigned int fpuGen = m_fpuGeneration.load(std::memory_order_relaxed);
			if (localFpuGeneration != fpuGen) {
				localFpuGeneration = fpuGen;
				ApplyFpuControlWord();
			}

			RunClaimedJobs(localGeneration);
		}
	}

	std::atomic<FJobFunc> m_pFunc;
	std::atomic<void *> m_pContext;
	std::atomic<int> m_jobCount;

	unsigned int m_fpuControlWord;
	std::atomic<unsigned int> m_fpuGeneration;

	//Bumped to publish a dispatch, and once more to publish shutdown
	std::atomic<unsigned int> m_generation;
	std::atomic<bool> m_stop;
	std::atomic<bool> m_faulted;

	//Generation in the high 32 bits, job index in the low 32: one atomic step does both.
	//
	//XXX: the generation half wraps after 2^32 dispatches. Unreachable in practice.
	unsigned char m_padA[JS_CACHE_LINE];
	std::atomic<unsigned long long> m_ticket;
	unsigned char m_padB[JS_CACHE_LINE];
	std::atomic<int> m_remaining;
	unsigned char m_padC[JS_CACHE_LINE];

	std::mutex m_mutex;
	std::condition_variable m_wake;

	std::mutex m_doneMutex;
	std::condition_variable m_done;

	std::thread *m_pThreads;
	int m_numWorkers;

	static_assert(alignof(decltype(m_ticket)) == 8,
		"m_ticket must be 8 byte aligned for its 64 bit atomic operations to be lock free; see the "
		"pack(8) at the top of this header and keep the standard includes above it");
};


#pragma pack(pop)

#endif //UTGLR_JOBSYSTEM_H
