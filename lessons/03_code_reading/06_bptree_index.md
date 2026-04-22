# 03-06. B+ Tree Index: id 검색을 빠르게 만들기

## 먼저 한 문장으로 보기
`bptree.c`는 `id`를 key로, 메모리 record 배열의 위치를 value로 저장해 `SELECT WHERE id = N`을 빠르게 처리하게 합니다.

## 요청 흐름에서의 위치
```text
SELECT * FROM users WHERE id = 1;
  -> SqlStatement where_type = SQL_WHERE_ID
  -> DbFilter type = DB_FILTER_ID
  -> db_select()
  -> bptree_search(id)
  -> record index
  -> records[record_index]
```

## 이 코드를 읽기 전에 알아야 할 CS 개념
| 개념 | 짧은 설명 | 이 파일에서 보이는 지점 |
| --- | --- | --- |
| index | 검색을 빠르게 하기 위해 별도로 유지하는 자료구조입니다. | `BPlusTree` |
| key/value mapping | key로 value를 찾는 관계입니다. | `id -> record index` |
| balanced tree | 삽입 후에도 높이를 과도하게 키우지 않는 tree입니다. | split과 promoted key |
| leaf node | 실제 value가 저장되는 말단 node입니다. | `values`, `next` |
| internal node | 검색 방향을 결정하는 node입니다. | `children` |

## 이 프로젝트의 B+ tree가 저장하는 것
이 구현은 record 전체를 B+ tree에 넣지 않습니다.

```text
key: id
value: record 배열의 index
```

예를 들어 `records[3]`에 `id = 10`인 row가 있다면 B+ tree에는 다음 정보가 들어갑니다.

```text
10 -> 3
```

이렇게 하면 index는 “어디에 있는지”만 알려주고, 실제 데이터는 record 배열에서 가져옵니다. 실제 DBMS의 인덱스도 보통 row 자체가 아니라 row를 찾기 위한 위치 정보를 담습니다.

## 주요 구조체: `BpNode`
- 이름: `BpNode`
- 위치: `include/bptree.h`
- 한 문장 목적: B+ tree의 node 하나를 표현합니다.
- 주요 필드:
  - `is_leaf`: leaf node인지 여부
  - `num_keys`: 현재 key 개수
  - `keys`: 정렬된 key 배열
  - `children`: internal node에서 child pointer 배열
  - `values`: leaf node에서 record index 배열
  - `next`: leaf node끼리 연결하는 pointer
- 학습자가 확인할 질문:
  - leaf node와 internal node가 같은 구조체를 공유하면 어떤 장단점이 있을까요?

## 주요 구조체: `BPlusTree`
- 이름: `BPlusTree`
- 위치: `include/bptree.h`
- 한 문장 목적: B+ tree 전체의 root와 key 개수를 보관합니다.
- 주요 필드:
  - `root`: root node pointer
  - `size`: 저장된 key 개수
- 학습자가 확인할 질문:
  - root가 바뀔 수 있다는 점은 tree 자료구조에서 왜 중요할까요?

## API 카드: `bptree_init`
- 이름: `bptree_init`
- 위치: `src/bptree.c`, 선언은 `include/bptree.h`
- 한 문장 목적: 비어 있는 B+ tree 상태를 만듭니다.
- 입력: `BPlusTree *tree`
- 출력: 없음
- 호출되는 시점: `db_init()`에서 DB engine이 index를 준비할 때
- 내부에서 하는 일:
  - `root`를 `NULL`로 둡니다.
  - `size`를 0으로 둡니다.
- 실패할 수 있는 지점:
  - 없음
- 학습자가 확인할 질문:
  - root가 `NULL`인 상태와 root가 leaf node인 상태는 어떻게 다를까요?

## API 카드: `bptree_insert`
- 이름: `bptree_insert`
- 위치: `src/bptree.c`
- 한 문장 목적: key와 value를 B+ tree에 삽입하고, 필요하면 root split을 처리합니다.
- 입력:
  - `BPlusTree *tree`
  - `int key`
  - `size_t value`
- 출력: 성공 시 `1`, 실패 시 `0`
- 호출되는 시점:
  - `db_init()`이 파일에서 기존 record를 읽어 index를 복구할 때
  - `db_insert()`가 새 record를 추가할 때
- 내부에서 하는 일:
  - root가 없으면 leaf root를 만듭니다.
  - `node_insert()`를 호출합니다.
  - split이 root까지 올라오면 새 root를 만듭니다.
  - 새 key면 `size`를 증가시킵니다.
- 실패할 수 있는 지점:
  - node 메모리 할당 실패
- 학습자가 확인할 질문:
  - 삽입 중 root가 바뀌는 상황은 왜 생길까요?

## API 카드: `bptree_search`
- 이름: `bptree_search`
- 위치: `src/bptree.c`
- 한 문장 목적: key를 찾아 연결된 value, 즉 record index를 반환합니다.
- 입력:
  - `const BPlusTree *tree`
  - `int key`
  - `size_t *value_out`
- 출력: 찾으면 `true`, 없으면 `false`
- 호출되는 시점: `db_select()`가 `DB_FILTER_ID`를 처리할 때
- 내부에서 하는 일:
  - root에서 시작합니다.
  - internal node에서는 key가 들어갈 child index를 고릅니다.
  - leaf node까지 내려갑니다.
  - leaf에서 key 위치를 찾아 value를 반환합니다.
- 실패할 수 있는 지점:
  - tree가 비어 있음
  - key가 없음
- 학습자가 확인할 질문:
  - B+ tree 검색은 왜 전체 record 배열을 처음부터 끝까지 보지 않아도 될까요?

## API 카드: `leaf_insert`
- 이름: `leaf_insert`
- 위치: `src/bptree.c`
- 한 문장 목적: leaf node에 key/value를 삽입하고, leaf가 꽉 차면 split 정보를 반환합니다.
- 입력: leaf node, key, value
- 출력: `InsertResult`
- 호출되는 시점: `node_insert()`가 leaf에 도착했을 때
- 내부에서 하는 일:
  - key가 들어갈 정렬 위치를 찾습니다.
  - 이미 같은 key가 있으면 value만 갱신합니다.
  - 공간이 있으면 배열을 밀고 삽입합니다.
  - 공간이 없으면 임시 배열에 key/value를 합친 뒤 반으로 나눕니다.
  - 새 right leaf를 만들고 `next` pointer를 연결합니다.
  - right leaf의 첫 key를 promoted key로 반환합니다.
- 실패할 수 있는 지점:
  - split용 right node 할당 실패
- 학습자가 확인할 질문:
  - B+ tree에서 leaf node들이 `next`로 연결되면 어떤 조회가 쉬워질까요?

## API 카드: `node_insert`
- 이름: `node_insert`
- 위치: `src/bptree.c`
- 한 문장 목적: 재귀적으로 삽입 위치를 찾고, child split이 올라오면 internal node에 반영합니다.
- 입력: 현재 node, key, value
- 출력: `InsertResult`
- 호출되는 시점: `bptree_insert()` 내부
- 내부에서 하는 일:
  - leaf면 `leaf_insert()`를 호출합니다.
  - internal node면 key에 맞는 child를 고릅니다.
  - child에서 split이 없으면 그대로 반환합니다.
  - child split이 있으면 promoted key와 right child를 현재 node에 삽입합니다.
  - 현재 node도 꽉 차면 internal split을 수행합니다.
- 실패할 수 있는 지점:
  - split용 right internal node 할당 실패
- 학습자가 확인할 질문:
  - split 정보가 아래에서 위로 올라오는 구조가 왜 필요할까요?

## split을 단계적으로 이해하기
B+ tree node는 key를 무한히 담지 않습니다. 이 프로젝트에서는 `BPTREE_MAX_KEYS`가 31입니다.

삽입으로 node가 꽉 차면:

1. 기존 key와 새 key를 임시 배열에 정렬된 상태로 모읍니다.
2. 왼쪽 node와 오른쪽 node로 나눕니다.
3. 오른쪽 node의 첫 key 또는 중간 key를 부모에게 올립니다.
4. 부모도 꽉 차면 split이 다시 위로 올라갑니다.
5. root까지 split되면 새 root가 생깁니다.

이 과정을 통해 tree는 한쪽으로만 길어지지 않고 균형을 유지합니다.

## 실제 DBMS와의 차이
이 프로젝트의 B+ tree는 메모리 기반입니다. 실제 DBMS의 B+ tree는 보통 디스크 page 단위로 관리됩니다.

차이점:

- 이 프로젝트: pointer로 child node를 연결합니다.
- 실제 DBMS: page id나 disk offset으로 child page를 찾습니다.
- 이 프로젝트: 서버 시작 시 파일을 읽어 index를 다시 만듭니다.
- 실제 DBMS: index 자체도 디스크에 저장하고 복구합니다.
- 이 프로젝트: id equality lookup만 사용합니다.
- 실제 DBMS: range scan, composite index, transaction, recovery와 연결됩니다.

## 코드 관찰 포인트
- `bptree_insert()`는 root가 없을 때 첫 leaf root를 만듭니다.
- `leaf_insert()`는 key를 정렬 위치에 넣고, 꽉 차면 leaf split을 수행합니다.
- `node_insert()`는 child split 결과를 부모에 반영합니다.
- `bptree_search()`는 internal node를 따라 leaf까지 내려간 뒤 key를 찾습니다.

## 흔한 오해
| 오해 | 바로잡기 |
| --- | --- |
| B+ tree에는 record 전체가 들어 있다. | 이 프로젝트에서는 record 배열의 index만 들어 있습니다. |
| split은 예외 상황이다. | split은 B+ tree가 균형을 유지하기 위한 정상 동작입니다. |
| index가 있으면 INSERT도 항상 빨라진다. | index는 검색을 빠르게 하지만, INSERT 때 index 갱신 비용을 추가합니다. |

## 다음 문서로 넘어가기
코드 독해의 핵심 축을 모두 봤습니다. 이제 이 최소구현을 출발점으로 사고를 확장합니다.

다음: [../04_expansion_questions.md](../04_expansion_questions.md)
