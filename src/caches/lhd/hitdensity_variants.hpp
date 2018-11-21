#pragma once

// Different object classification schemes for HitDensity.

namespace repl {

  namespace fn {

    class HitDensityBySize : public HitDensity {
      public:
        HitDensityBySize(cache::Cache* cache, const int numClasses, const uint64_t maxSize)
            : HitDensity(cache, numClasses) {

          uint32_t bound = maxSize;
          for (int i = 0; i < numClasses-1; i++) {
            sizeBounds.push_back(bound);
            bound /= 2;
          }
          std::reverse(sizeBounds.begin(), sizeBounds.end());

          std::cerr << "Size bounds: ";
          for (uint32_t i = 0; i < sizeBounds.size(); i++) {
              std::cerr << sizeBounds[i] << " ";
          }
          std::cerr << std::endl;
        }

        void update(candidate_t id, const parser::Request& req) {
          uint16_t clid = 0;
          while (clid < sizeBounds.size() && req.size() > sizeBounds[clid]) { ++clid; }
          // if (clid < sizeBounds.size()) {
          //     std::cerr << req.size() << " < " << sizeBounds[clid] << " ==> " << clid << std::endl;
          // } else {
          //     std::cerr << req.size() << " unbounded ==> " << clid << std::endl;
          // }
          classIds[id] = clid;
          HitDensity::update(id, req);
        }

      private:
        std::vector<uint32_t> sizeBounds;
    };

    class HitDensityByReuse : public HitDensity {
      public:
        HitDensityByReuse(cache::Cache* cache, uint32_t _maxReferenceCount)
            : HitDensity(cache, _maxReferenceCount + 1)
            , maxReferenceCount(_maxReferenceCount)
            , references(-1) {
          std::cerr << "HitDensityByReuse with maxReferenceCount: " << maxReferenceCount << std::endl;
        }

        void update(candidate_t id, const parser::Request& req) {
          references[id] += 1;
          HitDensity::update(id, req);
          classIds[id] = std::min(references[id], maxReferenceCount);
        }

        void replaced(candidate_t id) {
          references.erase(id);
          HitDensity::replaced(id);
        }

        void modelClassInteractions();

      private:
        const uint32_t maxReferenceCount;
        CandidateMap<uint32_t> references;
    };

    class HitDensityByApp : public HitDensity {
      public:
        HitDensityByApp(cache::Cache* cache, uint32_t _numAppClasses)
            : HitDensity(cache, _numAppClasses)
            , numAppClasses(_numAppClasses) {
          std::cerr << "HitDensityByApp with numAppClasses: " << numAppClasses << std::endl;
        }

        void update(candidate_t id, const parser::Request& req) {
          classIds[id] = req.appId % numAppClasses;
          HitDensity::update(id, req);
        }

        void modelClassInteractions();

      private:
        const uint32_t numAppClasses;
    };

    class HitDensityByAppAndReuse : public HitDensity {
      public:
        HitDensityByAppAndReuse(cache::Cache* cache, uint32_t _numAppClasses, uint32_t _maxReferenceCount)
            : HitDensity(cache, _numAppClasses * (_maxReferenceCount + 1))
            , numAppClasses(_numAppClasses)
            , maxReferenceCount(_maxReferenceCount)
            , references(-1) {
          std::cerr << "HitDensityByAppAndReuse with numAppClasses: " << numAppClasses << " and maxReferenceCount: " << maxReferenceCount << std::endl;
        }

        void update(candidate_t id, const parser::Request& req) {
          classIds[id] = getClassId(id, req);
          references[id] += 1;
          HitDensity::update(id, req);
          classIds[id] = getClassId(id, req);
        }

        void replaced(candidate_t id) {
          references.erase(id);
          HitDensity::replaced(id);
        }

        void modelClassInteractions();

      private:
        inline void getAppAndRefsFromClassid(uint32_t classId,
                                             uint32_t& appClass,
                                             uint32_t& refClass) const {
            refClass = classId / numAppClasses;
            appClass = classId % numAppClasses;
        }
        inline uint32_t getClassId(uint32_t appClass, uint32_t refClass) const {
            return refClass * numAppClasses + appClass;
        }
        inline uint32_t getClassId(candidate_t id, const parser::Request& req) {
            uint32_t refClass = std::min(references[id], maxReferenceCount);
            uint32_t appClass = req.appId % numAppClasses;
            return getClassId(appClass, refClass);
        }
        
        const uint32_t numAppClasses;
        const uint32_t maxReferenceCount;
        CandidateMap<uint32_t> references;
    };

    class HitDensityNoSize : public HitDensity {
      public:
        HitDensityNoSize(cache::Cache* cache)
          : HitDensity(cache) {
          }

        rank_t rank (candidate_t id) {
          uint64_t a = HitDensity::age(id);

          // it can never be allowed for the saturating age to have highest
          // rank, or the cache can get stuck with all lines saturated
          if (a == MAX_COARSENED_AGE - 1) {
            return std::numeric_limits<rank_t>::max();
          }

          hitdensity::Class* c = HitDensity::cl(id);
          // uint32_t s = cache->getSize(id); assert(s != -1u);
          uint32_t s = 1;

          // compute HitDensity based on the size!
          rank_t rank = c->expectedLifetime[a] > 0?
              (c->hitProbability[a] / (s * c->expectedLifetime[a]))
              : 0.;

          // Higher HitDensity is better, so return its negation since higher rank is BAD!!!
          return -rank; // eliminate errors within 1e-3
        }
      };

      class HitDensityByAppAndReuseNoSize : public HitDensityByAppAndReuse {
        public:
          HitDensityByAppAndReuseNoSize(cache::Cache* cache, uint32_t _numAppClasses, uint32_t _maxReferenceCount)
            : HitDensityByAppAndReuse(cache, _numAppClasses, _maxReferenceCount) {
            }

          rank_t rank (candidate_t id) {
            uint64_t a = HitDensity::age(id);

            // it can never be allowed for the saturating age to have highest
            // rank, or the cache can get stuck with all lines saturated
            if (a == MAX_COARSENED_AGE - 1) {
              return std::numeric_limits<rank_t>::max();
            }

            hitdensity::Class* c = HitDensity::cl(id);
            uint32_t s = 1;

            // compute HitDensity based on the size!
            rank_t rank = c->expectedLifetime[a] > 0?
                (c->hitProbability[a] / (s * c->expectedLifetime[a]))
                : 0.;

            // Higher HitDensity is better, so return its negation since higher rank is BAD!!!
            return -rank; // eliminate errors within 1e-3
          }
      };

  } // namespace fn

  typedef RankedPolicy<fn::HitDensityBySize> RankedHitDensityBySize;
  typedef RankedPolicy<fn::HitDensityByReuse> RankedHitDensityByReuse;
  typedef RankedPolicy<fn::HitDensityByApp> RankedHitDensityByApp;
  typedef RankedPolicy<fn::HitDensityByAppAndReuse> RankedHitDensityByAppAndReuse;
  typedef RankedPolicy<fn::HitDensityNoSize> RankedHitDensityNoSize;
  typedef RankedPolicy<fn::HitDensityByAppAndReuseNoSize> RankedHitDensityByAppAndReuseNoSize;

} // namespace repl
